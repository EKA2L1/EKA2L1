// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <dispatch/tls.h>
#include <dispatch/dispatcher.h>
#include <common/log.h>
#include <config/config.h>
#include <kernel/kernel.h>
#include <kernel/process.h>
#include <system/epoc.h>
#include <utils/err.h>

#include <algorithm>
#include <cstring>
#include <limits>

namespace eka2l1::dispatch {
    std::int32_t tls_controller::create(std::uint64_t owner, const std::string &hostname, const std::string &peer_address) {
        if (sessions_.size() >= 256 || next_handle_ > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
            std::count_if(sessions_.begin(), sessions_.end(), [owner](const auto &item) { return item.second.owner == owner; }) >= 64) {
            return epoc::error_no_memory;
        }
        auto session = std::make_unique<drivers::tls_session>();
        if (!session->configure(hostname, peer_address)) {
            LOG_ERROR(HLE_DISPATCHER, "Cannot initialize TLS for {}: {}", hostname, session->error());
            return epoc::error_not_ready;
        }
        const auto handle = next_handle_++;
        sessions_.emplace(handle, entry{owner, std::move(session)});
        return static_cast<std::int32_t>(handle);
    }

    drivers::tls_session *tls_controller::get(std::uint64_t owner, std::uint32_t handle) {
        const auto found = sessions_.find(handle);
        return found != sessions_.end() && found->second.owner == owner ? found->second.session.get() : nullptr;
    }

    bool tls_controller::erase(std::uint64_t owner, std::uint32_t handle) {
        return get(owner, handle) && sessions_.erase(handle);
    }

    void tls_controller::erase_process(std::uint64_t owner) {
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            if (it->second.owner == owner) {
                it = sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, etls_create, epoc::desc8 *hostname, epoc::desc8 *peer_address) {
        auto *process = sys->get_kernel_system()->crr_process();
        if (!hostname || hostname->get_length() == 0 || hostname->get_length() > 253 || !hostname->get_pointer_raw(process)) {
            return epoc::error_argument;
        }
        if (!peer_address || peer_address->get_length() == 0 || peer_address->get_length() > 64
            || !peer_address->get_pointer_raw(process)) {
            return epoc::error_argument;
        }
        auto server_name = hostname->to_std_string(process);
        if (const auto target = sys->get_config()->host_override(server_name);
            target && config::valid_host_name(*target) && !config::numeric_host_address(*target)) {
            server_name = *target;
        }
        return sys->get_dispatcher()->get_tls_controller().create(process->unique_id(), server_name, peer_address->to_std_string(process));
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, etls_destroy, std::uint32_t handle) {
        return sys->get_dispatcher()->get_tls_controller().erase(sys->get_kernel_system()->crr_process()->unique_id(), handle)
            ? epoc::error_none : epoc::error_bad_handle;
    }

    BRIDGE_FUNC_DISPATCHER(std::int32_t, etls_command, std::uint32_t handle, std::uint32_t operation,
        epoc::desc8 *input, epoc::des8 *output) {
        auto *process = sys->get_kernel_system()->crr_process();
        auto *session = sys->get_dispatcher()->get_tls_controller().get(process->unique_id(), handle);
        if (!session) {
            return epoc::error_bad_handle;
        }
        const auto input_size = input ? input->get_length() : 0;
        const auto output_size = output ? output->get_max_length(process) : 0;
        auto *source = input ? reinterpret_cast<const std::uint8_t *>(input->get_pointer_raw(process)) : nullptr;
        auto *dest = output ? reinterpret_cast<std::uint8_t *>(output->get_pointer_raw(process)) : nullptr;
        if (input_size > 65536 || output_size > 65536 || (input_size && !source) || (output_size && !dest)) {
            return epoc::error_argument;
        }
        auto copy_output = [&](const std::uint8_t *data, std::size_t size) {
            if (!output || size > output_size) {
                return static_cast<std::int32_t>(epoc::error_overflow);
            }
            if (size) {
                std::memcpy(dest, data, size);
            }
            output->set_length(process, static_cast<std::uint32_t>(size));
            return static_cast<std::int32_t>(size);
        };
        int result;
        switch (operation) {
        case 0:
            result = session->handshake();
            if (result == static_cast<int>(drivers::tls_result::failed) || result == static_cast<int>(drivers::tls_result::verification_failed)) {
                LOG_ERROR(HLE_DISPATCHER, "TLS handshake failed: {}", session->error());
            }
            return result;
        case 1:
            return session->feed(source, input_size) ? epoc::error_none : epoc::error_overflow;
        case 2:
            if (!output) {
                return static_cast<std::int32_t>(session->pending_output());
            }
            result = static_cast<int>(session->drain(dest, output_size));
            output->set_length(process, result);
            return result;
        case 3:
            if (!output) {
                return epoc::error_argument;
            }
            result = session->read(dest, output_size);
            output->set_length(process, result > 0 ? result : 0);
            return result;
        case 4:
            return session->write(source, input_size);
        case 5: {
            const auto protocol = session->protocol();
            return copy_output(reinterpret_cast<const std::uint8_t *>(protocol.data()), protocol.size());
        }
        case 6: {
            const auto cipher = session->cipher_suite();
            const std::uint8_t bytes[] = {static_cast<std::uint8_t>(cipher >> 8), static_cast<std::uint8_t>(cipher)};
            return copy_output(bytes, sizeof(bytes));
        }
        case 7: {
            const auto cert = session->peer_certificate();
            return output ? copy_output(cert.data(), cert.size()) : static_cast<int>(cert.size());
        }
        case 8:
            return session->close_notify();
        case 9:
            session->transport_closed();
            return epoc::error_none;
        default:
            return epoc::error_not_supported;
        }
    }
}
