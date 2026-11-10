// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <control/ngage.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <system/epoc.h>
#include <vfs/vfs.h>
#include <atomic>
#include <chrono>
#include "ngage_agent.h"

namespace eka2l1::control {
    static const std::u16string executable = u"C:\\sys\\bin\\eka2l1_ngage_launch.exe";
    static const std::u16string response_path = u"C:\\private\\e000ea02\\response.bin";

    struct ngage_controller::impl {
        struct completion {
            std::atomic<bool> finished{ false };
            int error = -33;
        };
        std::shared_ptr<completion> pending;
        kernel::uid thread_id = 0;
        std::uint32_t game_uid = 0;
        std::string command;
        std::chrono::steady_clock::time_point deadline;

        void cancel(kernel_system *kern) {
            if (pending && !pending->finished.load()) {
                auto *thread = kern->get_by_id<kernel::thread>(thread_id);
                if (thread)
                    thread->owning_process()->kill(kernel::entity_exit_type::terminate, u"Cancelled", -33);
            }
            pending.reset();
        }
    };

    ngage_controller::ngage_controller() : impl_(std::make_unique<impl>()) {}
    ngage_controller::~ngage_controller() = default;

    void ngage_controller::clear() { impl_ = std::make_unique<impl>(); }

    void ngage_controller::cancel(system &sys) {
        if (auto *kern = sys.get_kernel_system()) {
            kernel_lock guard(kern);
            impl_->cancel(kern);
        }
    }

    void ngage_controller::reset(system &sys) {
        auto *kern = sys.get_kernel_system();
        if (!kern) {
            clear();
            return;
        }
        kernel_lock guard(kern);
        impl_->cancel(kern);
        if (impl_->game_uid) {
            for (auto &object : kern->get_process_list()) {
                auto *game = static_cast<kernel::process *>(object.get());
                if (std::get<2>(game->get_uid_type()) == impl_->game_uid
                    && game->get_exit_type() == kernel::entity_exit_type::pending) {
                    game->kill(kernel::entity_exit_type::kill, u"Closed", 0);
                    break;
                }
            }
            impl_->game_uid = 0;
        }
    }

    response ngage_controller::request(system &sys, const std::string &command, bool poll) {
        auto &state = *impl_;
        auto *io = sys.get_io_system();
        auto *kern = sys.get_kernel_system();
        if (!poll) {
            if (state.pending)
                return response::error(409, "busy", "another Games request is in flight");
            const auto destination = io->get_raw_path(executable);
            if (!destination)
                return response::error(409, "no_device", "no writable system drive");
            const auto host_path = common::ucs2_to_utf8(*destination);
            common::create_directories(file_directory(host_path));
            auto *file = common::open_c_file(host_path, "wb");
            if (!file)
                return response::error(422, "ngage_failed", "cannot stage Games IPC agent");
            const bool written = fwrite(ngage_agent, 1, sizeof(ngage_agent), file) == sizeof(ngage_agent);
            const int closed = fclose(file);
            if (!written || closed)
                return response::error(422, "ngage_failed", "cannot write Games IPC agent");
            if (io->exist(response_path))
                io->delete_entry(response_path);
            state.command = command;
            state.pending = std::make_shared<impl::completion>();
            state.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            kernel_lock guard(kern);
            auto *process = kern->spawn_new_process(executable, common::utf8_to_ucs2(command), 0, 0x10000);
            if (!process) {
                state.pending.reset();
                return response::error(422, "ngage_failed", "cannot create Games IPC agent");
            }
            state.thread_id = process->get_primary_thread()->unique_id();
            process->logon([result = state.pending](kernel::process *exited) {
                result->error = exited->get_exit_reason();
                if (!result->error && exited->get_exit_type() == kernel::entity_exit_type::panic)
                    result->error = -2;
                result->finished.store(true);
            });
            if (!process->run()) {
                process->kill(kernel::entity_exit_type::terminate, u"Launch", -2);
                state.pending.reset();
                return response::error(422, "ngage_failed", "cannot run Games IPC agent");
            }
            kern->stop_cores_idling();
        }
        if (!state.pending || command != state.command)
            return response::error(409, "cancelled", "Games request cancelled by a session change");
        if (!state.pending->finished.load()) {
            if (std::chrono::steady_clock::now() < state.deadline)
                return { 202, "application/json", "{}", {} };
            kernel_lock guard(kern);
            state.cancel(kern);
            return response::error(504, "timeout", "Games IPC request timed out");
        }
        const int error = state.pending->error;
        state.pending.reset();
        if (error)
            return response::error(error == -1 ? 404 : 422, "ngage_failed", "PlayServer request failed (Symbian error " + std::to_string(error) + ")");

        const auto path = io->get_raw_path(response_path);
        std::vector<std::uint8_t> data;
        if (path) {
            auto *file = common::open_c_file(common::ucs2_to_utf8(*path), "rb");
            if (file) {
                std::uint8_t buffer[4096];
                std::size_t size;
                while ((size = fread(buffer, 1, sizeof(buffer), file)) != 0 && data.size() < 128 * 1024)
                    data.insert(data.end(), buffer, buffer + size);
                fclose(file);
            }
        }
        std::size_t offset = 0;
        const auto word = [&]() -> std::uint32_t {
            if (offset + 4 > data.size())
                throw api_error(422, "ngage_failed", "truncated GamesList response");
            std::uint32_t value = 0;
            for (int i = 0; i < 4; ++i)
                value |= std::uint32_t(data[offset++]) << (i * 8);
            return value;
        };
        if (word() != 0x32474E45 || word() != 1)
            throw api_error(422, "ngage_failed", "invalid GamesList response");
        const auto count = word();
        if (count > 256)
            throw api_error(422, "ngage_failed", "invalid GamesList count");
        json::array games;
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto uid = word(), length = word();
            if (length > 128 || offset + length * 2 > data.size())
                throw api_error(422, "ngage_failed", "invalid GamesList title");
            std::u16string title;
            for (std::uint32_t j = 0; j < length; ++j) {
                title += char16_t(data[offset] | std::uint16_t(data[offset + 1]) << 8);
                offset += 2;
            }
            games.push_back({ { "uid", hex(uid) }, { "name", common::ucs2_to_utf8(title) } });
        }
        if (offset != data.size() || (command != "list" && count))
            throw api_error(422, "ngage_failed", "unexpected GamesList data");
        if (command == "list")
            return response::from_json({ { "games", games } });
        state.game_uid = parse_uid("0x" + command.substr(7));
        return response::from_json({ { "uid", hex(state.game_uid) } });
    }
}
