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
#include <loader/rsc.h>
#include <services/ui/view/view.h>

#include <system/epoc.h>
#include <utils/err.h>
#include <vfs/vfs.h>

namespace eka2l1 {
    std::string get_view_server_name_by_epocver(const epocver ver) {
        if (ver <= epocver::eka2) {
            return "ViewServer";
        }

        return "!ViewServer";
    }

    view_server::view_server(system *sys)
        : service::typical_server(sys, get_view_server_name_by_epocver(sys->get_symbian_version_use()))
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
            LOG_WARN(SERVICE_UI, "Can't find priority set resource file for view server! Priority set to standard 0.");
            return true;
        }

        eka2l1::ro_file_stream fstream(f.get());
        loader::rsc_file rsc_priority(reinterpret_cast<common::ro_stream *>(&fstream));

        auto priority_view_value_raw = rsc_priority.read(2);
        priority_ = *reinterpret_cast<const std::uint32_t *>(priority_view_value_raw.data());

        flags_ |= flag_inited;

        return true;
    }

    void view_server::add_view(const ui::view::view_id &new_id) {
        ids_.push_back(new_id);
    }

    void view_server::deactivate() {
        static constexpr std::size_t VIEW_NOT_FOUND = 0xFFFFFFFF;

        // Find the next id to activate
        std::size_t pos_of_current_view = VIEW_NOT_FOUND;
        auto ite = std::find(ids_.begin(), ids_.end(), active_);

        if (ite != ids_.end()) {
            pos_of_current_view = std::distance(ids_.begin(), ite);
        }

        if (ids_.size() == 1) {
            active_ = ui::view::view_id { 0, 0 };
        } else {
            if ((pos_of_current_view == VIEW_NOT_FOUND) || (pos_of_current_view < (ids_.size() - 1))) {
                // Choose the lastest added one as active
                active_ = ids_.back();
            } else {
                // The one being activated is already the lastest one. Choose the one before the lastest
                active_ = ids_[ids_.size() - 2];
            }
        }
    }

    view_session::view_session(service::typical_server *server, const kernel::uid session_uid, epoc::version client_version)
        : service::typical_session(server, session_uid, client_version)
        , to_panic_(nullptr)
        , app_uid_(0) {
    }

    void view_session::add_view(service::ipc_context *ctx) {
        std::optional<ui::view::view_id> id = ctx->get_argument_data_from_descriptor<ui::view::view_id>(0);

        if (!id) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (app_uid_ == 0) {
            app_uid_ = id->app_uid;
        } else {
            if (app_uid_ != id->app_uid) {
                ctx->complete(epoc::error_argument);
                return;
            }
        }

        server<view_server>()->add_view(id.value());
        ctx->complete(epoc::error_none);
    }

    void view_session::request_view_event(service::ipc_context *ctx) {
        const std::size_t event_buf_size = ctx->get_argument_data_size(0);

        if (event_buf_size < sizeof(ui::view::view_event)) {
            LOG_ERROR(SERVICE_UI, "Size of view event buffer is not sufficient enough!");
            ctx->complete(epoc::error_argument);
            return;
        }

        std::uint8_t *event_buf = ctx->get_descriptor_argument_ptr(0);
        epoc::notify_info nof_info(ctx->msg->request_sts, ctx->msg->own_thr);

        if (!queue_.hear(nof_info, event_buf)) {
            ctx->complete(epoc::error_already_exists);
            return; // TODO: We can panic. It's allowed.
        }
    }

    void view_session::active_view(service::ipc_context *ctx, const bool /*should_complete*/) {
        kernel_system *kern = server<view_server>()->get_kernel_object_owner();
        
        std::optional<epoc::uid> custom_message_uid;
        std::optional<ui::view::view_id> id;
        std::uint8_t *custom_message_buf = nullptr;
        std::size_t custom_message_size = 0;

        if (kern->is_eka1()) {
            struct active_view_fundamental_info {
                ui::view::view_id app_id_;
                epoc::uid custom_message_uid_;
                std::uint32_t custom_message_size_;
            };

            std::optional<active_view_fundamental_info> info = ctx->get_argument_data_from_descriptor<active_view_fundamental_info>(0);
            if (!info) {
                ctx->complete(epoc::error_argument);
                return;
            }

            id = info->app_id_;
            custom_message_uid = info->custom_message_uid_;
            custom_message_size = info->custom_message_size_;
            custom_message_buf = ctx->get_descriptor_argument_ptr(1);
        } else {
            custom_message_uid = ctx->get_argument_value<epoc::uid>(1);
            custom_message_buf = ctx->get_descriptor_argument_ptr(2);
            custom_message_size = ctx->get_argument_data_size(2);
            id = ctx->get_argument_data_from_descriptor<ui::view::view_id>(0);
        }

        if (!id || !custom_message_uid) {
            ctx->complete(epoc::error_argument);
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
        ctx->complete(epoc::error_none);
    }

    void view_session::deactive_view(service::ipc_context *ctx, const bool /*should_complete*/) {
        view_server *svr = server<view_server>();

        const ui::view::view_id view_to_deactivate = svr->active_view();
        
        // Deactivate view then get the one currently being activated
        svr->deactivate();
        const ui::view::view_id view_that_activated = svr->active_view();

        queue_.queue_event({ ui::view::view_event::event_deactive_view, view_to_deactivate, view_that_activated,
            0, 0 });
        ctx->complete(epoc::error_none);
    }

    void view_session::async_message_for_client_to_panic_with(service::ipc_context *ctx) {
        to_panic_ = ctx->msg;
        ctx->auto_free = false;
    }

    void view_session::get_priority(service::ipc_context *ctx) {
        const std::uint32_t priority = server<view_server>()->priority();
        ctx->write_data_to_descriptor_argument(0, &priority);
        ctx->complete(epoc::error_none);
    }

    void view_session::fetch(service::ipc_context *ctx) {
        kernel_system *kern = server<view_server>()->get_kernel_object_owner();

        if (!kern->is_eka1() && (ctx->msg->function >= view_opcode_set_system_default_view)) {
            // This is moved for capability safety in EKA2...
            ctx->msg->function += 1;
        }

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

        case view_opcode_deactivate_active_view: {
            deactive_view(ctx, false);
            break;
        }

        case view_opcode_create_activate_view_event: {
            active_view(ctx, false);
            break;
        }

        case view_opcode_create_deactivate_view_event:
            deactive_view(ctx, false);
            break;

        case view_opcode_set_background_color: {
            LOG_WARN(SERVICE_UI, "SetBackgroundColor stubbed");
            ctx->complete(epoc::error_none);
            break;
        }

        default:
            LOG_ERROR(SERVICE_UI, "Unimplemented view session opcode {}", ctx->msg->function);
            break;
        }
    }
}