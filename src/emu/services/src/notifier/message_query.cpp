/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <services/notifier/notifier.h>
#include <services/notifier/queries.h>

#include <common/buffer.h>
#include <common/unicode.h>
#include <drivers/ui/input_dialog.h>
#include <kernel/kernel.h>
#include <loader/rsc.h>
#include <system/epoc.h>
#include <utils/bafl.h>
#include <utils/err.h>
#include <vfs/vfs.h>

#include <array>

namespace eka2l1 {
    namespace {
        template <typename T>
        bool read_value(common::ro_stream &stream, T &value) {
            return stream.read(&value, sizeof(value)) == sizeof(value);
        }

        bool read_query_text(common::ro_buf_stream &stream, std::uint8_t *data, std::u16string &text) {
            std::int16_t length = 0;
            std::uint8_t first = 0;
            if (!read_value(stream, length) || length < 0 || !read_value(stream, first)) {
                return false;
            }

            // Streamed descriptors encode character width and length in a tagged cardinality.
            const unsigned bytes = !(first & 1) ? 1 : !(first & 2) ? 2 : !(first & 4) ? 4 : 0;
            std::uint32_t cardinality = first;
            if (!bytes || stream.read(reinterpret_cast<std::uint8_t *>(&cardinality) + 1, bytes - 1) != bytes - 1) {
                return false;
            }
            cardinality >>= bytes == 1 ? 1 : bytes == 2 ? 2 : 3;
            if ((cardinality >> 1) > static_cast<std::uint32_t>(length)) {
                return false;
            }

            length = static_cast<std::int16_t>(cardinality >> 1);
            text.resize(length);
            if (cardinality & 1) {
                for (auto &character : text) {
                    std::uint8_t value = 0;
                    if (!read_value(stream, value)) {
                        return false;
                    }
                    character = value;
                }
                return true;
            }
            int consumed = static_cast<int>(stream.left());
            common::unicode_expander expander;
            if (expander.expand(data + stream.tell(), consumed, reinterpret_cast<std::uint8_t *>(text.data()),
                    length * sizeof(char16_t)) != length * sizeof(char16_t)) {
                return false;
            }
            stream.seek(consumed, common::seek_where::cur);
            return true;
        }

        bool read_query_buttons(eka2l1::system *sys, std::int32_t resource, std::array<epoc::notifier::message_query_button, 2> &buttons) {
            auto *io = sys->get_io_system();
            const auto path = utils::get_nearest_lang_file(io, u"Z:\\resource\\avkon.rsc", sys->get_system_language(), drive_z);
            auto file = io->open_file(path, READ_MODE | BIN_MODE);
            if (!file) {
                return false;
            }
            ro_file_stream stream(file.get());
            loader::rsc_file resources(&stream);
            if (!resources.confirm_signature()) {
                return false;
            }
            auto data = resources.read(resource);
            return epoc::notifier::read_message_query_buttons(data.data(), data.size(), buttons);
        }
    }

    bool epoc::notifier::read_message_query_buttons(std::uint8_t *data, std::size_t size, std::array<message_query_button, 2> &buttons) {
        if (!data || size < 12) {
            return false;
        }
        common::ro_buf_stream cba(data, size);
        std::uint32_t flags = 0, related = 0;
        std::uint16_t breadth = 0, count = 0;
        if (!read_value(cba, flags) || !read_value(cba, breadth) || !read_value(cba, related)
            || !read_value(cba, count) || count < buttons.size()) {
            return false;
        }
        for (auto &button : buttons) {
            std::uint8_t version = 0;
            std::int16_t longpress = 0, bitmap = 0, mask = 0;
            std::u16string bitmap_file;
            if (!read_value(cba, version) || (version != 0 && version != 10)
                || !read_value(cba, button.command) || (version == 10 && !read_value(cba, longpress))
                || !loader::read_resource_string(cba, button.text)
                || !loader::read_resource_string(cba, bitmap_file)
                || !read_value(cba, bitmap) || !read_value(cba, mask)) {
                return false;
            }
        }
        return true;
    }

    bool epoc::notifier::read_message_query(std::uint8_t *data, std::size_t size, message_query &query) {
        if (!data || size < 14) {
            return false;
        }
        common::ro_buf_stream stream(data, size);
        std::int32_t signature = 0;
        std::int16_t image = 0, mask = 0, tone = 0;
        return read_value(stream, signature) && signature == 'a' * 'k' * 'n'
            && read_value(stream, query.softkeys) && read_value(stream, image)
            && read_value(stream, mask) && read_value(stream, tone)
            && read_query_text(stream, data, query.text) && read_query_text(stream, data, query.header);
    }

    bool notifier_server::acquire_message_query(const std::shared_ptr<epoc::notify_info> &query) {
        const auto pending = message_query_.lock();
        if (pending && !pending->empty()) {
            return false;
        }
        message_query_ = query;
        return true;
    }

    void notifier_client_session::start_message_query(service::ipc_context *ctx) {
        auto query_completion = std::make_shared<epoc::notify_info>(ctx->msg->request_sts, ctx->msg->own_thr);
        if (!server<notifier_server>()->acquire_message_query(query_completion)) {
            ctx->complete(epoc::error_in_use);
            return;
        }
        epoc::notifier::message_query query;
        auto *data = ctx->get_descriptor_argument_ptr(1);
        if (!data || !epoc::notifier::read_message_query(data, ctx->get_argument_data_size(1), query)) {
            ctx->complete(epoc::error_argument);
            return;
        }
        std::array<epoc::notifier::message_query_button, 2> buttons;
        if (!read_query_buttons(ctx->sys, query.softkeys, buttons)) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (buttons[0].text.empty()) {
            std::swap(buttons[0], buttons[1]);
        }
        if (buttons[0].text.empty()) {
            ctx->complete(epoc::error_not_supported);
            return;
        }
        message_query_ = std::move(query_completion);
        message_query_requester_ = ctx->msg->own_thr->unique_id();
        const std::weak_ptr<epoc::notify_info> completion = message_query_;
        const kernel::uid requester_id = message_query_requester_;
        auto *kern = ctx->sys->get_kernel_system();
        cancel_message_query_dialog_ = drivers::ui::show_yes_no_dialog(query.header + u'\n' + query.text, buttons[0].text, buttons[1].text,
            [kern, completion, requester_id, buttons](int selected) {
                if (completion.expired()) {
                    return;
                }
                const std::lock_guard<kernel_system> guard(*kern);
                const auto pending = completion.lock();
                const auto *requester = kern->get_by_id<kernel::thread>(requester_id);
                if (!pending || pending->empty() || !requester || requester->current_state() == kernel::thread_state::stop) {
                    return;
                }
                // Avkon completes the request status with the command, leaving the reply descriptor untouched.
                pending->complete(selected >= 0 && selected < static_cast<int>(buttons.size()) ? buttons[selected].command : epoc::error_cancel);
            });
        if (!cancel_message_query_dialog_) {
            message_query_->complete(epoc::error_not_ready);
        }
    }

    void notifier_client_session::cancel_message_query() {
        if (message_query_ && !message_query_->empty()) {
            if (cancel_message_query_dialog_) {
                cancel_message_query_dialog_();
            }
            auto *kern = server<notifier_server>()->get_kernel_object_owner();
            const auto *requester = kern->get_by_id<kernel::thread>(message_query_requester_);
            if (requester && requester->current_state() != kernel::thread_state::stop) {
                message_query_->complete(epoc::error_cancel);
            }
        }
        message_query_.reset();
        cancel_message_query_dialog_ = {};
    }
}
