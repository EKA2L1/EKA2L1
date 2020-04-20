/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/log.h>
#include <epoc/loader/rsc.h>
#include <epoc/services/ui/view/view.h>

#include <epoc/epoc.h>
#include <epoc/utils/err.h>
#include <epoc/vfs.h>

namespace eka2l1 {
    view_server::view_server(system *sys)
        : service::typical_server(sys, "!ViewServer")
        , flags_(0)
        , active_{ 0, 0 } {
    }

    void view_server::connect(service::ipc_context &ctx) {
        if (!(flags_ & flag_inited)) {
            init(sys->get_io_system());
        }

        create_session<view_session>(&ctx);
        typical_server::connect(ctx);
    }

    bool view_server::init(io_system *io) {
        priority_ = 0;

        // Try to read resource file contains priority
        std::u16string priority_filename = u"resource\\apps\\PrioritySet.rsc";
        symfile f = nullptr;

        for (drive_number drive = drive_z; drive >= drive_a; drive = static_cast<drive_number>(static_cast<int>(drive) - 1)) {
            if (io->get_drive_entry(drive)) {
                f = io->open_file(std::u16string(drive_to_char16(drive), 1) + priority_filename, READ_MODE | BIN_MODE);

                if (f) {
                    break;
                }
            }
        }

        if (!f) {
            LOG_WARN("Can't find priority set resource file for view server! Priority set to standard 0.");
            return true;
        }

        eka2l1::ro_file_stream fstream(f.get());
        loader::rsc_file rsc_priority(reinterpret_cast<common::ro_stream *>(&fstream));

        auto priority_view_value_raw = rsc_priority.read(2);
        priority_ = *reinterpret_cast<const std::uint32_t *>(priority_view_value_raw.data());

        flags_ |= flag_inited;

        return true;
    }

    view_session::view_session(service::typical_server *server, const service::uid session_uid, epoc::version client_version)
        : service::typical_session(server, session_uid, client_version)
        , to_panic_(nullptr)
        , app_uid_(0) {
    }

    void view_session::add_view(service::ipc_context *ctx) {
        std::optional<ui::view::view_id> id = ctx->get_arg_packed<ui::view::view_id>(0);

        if (!id) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        if (app_uid_ == 0) {
            app_uid_ = id->app_uid;
        } else {
            if (app_uid_ != id->app_uid) {
                ctx->set_request_status(epoc::error_argument);
                return;
            }
        }

        ids_.push_back(id.value());
        ctx->set_request_status(epoc::error_none);
    }

    void view_session::request_view_event(service::ipc_context *ctx) {
        const std::size_t event_buf_size = ctx->get_arg_size(0);

        if (event_buf_size < sizeof(ui::view::view_event)) {
            LOG_ERROR("Size of view event buffer is not sufficent enough!");
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        std::uint8_t *event_buf = ctx->get_arg_ptr(0);
        epoc::notify_info nof_info(ctx->msg->request_sts, ctx->msg->own_thr);

        if (!queue_.hear(nof_info, event_buf)) {
            ctx->set_request_status(epoc::error_already_exists);
            return; // TODO: We can panic. It's allowed.
        }
    }

    void view_session::active_view(service::ipc_context *ctx, const bool /*should_complete*/) {
        std::optional<epoc::uid> custom_message_uid = ctx->get_arg<epoc::uid>(1);
        std::uint8_t *custom_message_buf = ctx->get_arg_ptr(2);
        const std::size_t custom_message_size = ctx->get_arg_size(2);
        std::optional<ui::view::view_id> id = ctx->get_arg_packed<ui::view::view_id>(0);

        if (!id || !custom_message_uid) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        // TODO: More strict view switching.
        // Currently views between apps can be switched freely, but that will cause chaos
        std::vector<std::uint8_t> custom_message_buf_cop;
        custom_message_buf_cop.resize(custom_message_size);

        if (custom_message_buf && custom_message_size != 0) {
            std::copy(custom_message_buf, custom_message_buf + custom_message_size, &custom_message_buf_cop[0]);
        }

        customs_.push(custom_message_buf_cop);

        queue_.queue_event({ ui::view::view_event::event_active_view, id.value(), server<view_server>()->active_view(),
            custom_message_uid.value(), static_cast<std::int32_t>(custom_message_size) });

        server<view_server>()->set_active(id.value());
        ctx->set_request_status(epoc::error_none);
    }

    void view_session::async_message_for_client_to_panic_with(service::ipc_context *ctx) {
        to_panic_ = ctx->msg;
        ctx->auto_free = false;
    }

    void view_session::get_priority(service::ipc_context *ctx) {
        const std::uint32_t priority = server<view_server>()->priority();
        ctx->write_arg_pkg(0, &priority);
        ctx->set_request_status(epoc::error_none);
    }

    void view_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case view_opcode_async_msg_for_client_to_panic: {
            async_message_for_client_to_panic_with(ctx);
            break;
        }

        case view_opcode_priority: {
            get_priority(ctx);
            break;
        }

        case view_opcode_request_view_event: {
            request_view_event(ctx);
            break;
        }

        case view_opcode_add_view: {
            add_view(ctx);
            break;
        }

        case view_opcode_active_view: {
            active_view(ctx, true);
            break;
        }

        case view_opcode_create_activate_view_event: {
            active_view(ctx, false);
            break;
        }

        case view_opcode_set_background_color: {
            LOG_WARN("SetBackgroundColor stubbed");
            ctx->set_request_status(epoc::error_none);
            break;
        }

        default:
            LOG_ERROR("Unimplemented view session opcode {}", ctx->msg->function);
            break;
        }
    }
}