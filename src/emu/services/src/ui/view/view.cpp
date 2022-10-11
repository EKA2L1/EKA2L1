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
        if (ver <= epocver::epoc80) {
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

    std::optional<ui::view::view_id> view_server::active_view() {
        for (auto &[uid, session]: sessions) {
            view_session *ss = reinterpret_cast<view_session*>(session.get());
            if (ss) {
                std::optional<ui::view::view_id> id = ss->active_view();
                if (id.has_value()) {
                    return id;
                }
            }
        }

        return std::nullopt;
    }

    view_session *view_server::active_view_session() {
        for (auto &[uid, session]: sessions) {
            view_session *ss = reinterpret_cast<view_session*>(session.get());
            if (ss && ss->active_view().has_value()) {
                return ss;
            }
        }

        return nullptr;
    }

    void view_server::call_activation_listener(const ui::view::view_id id) {
        for (auto &[uid, session]: sessions) {
            view_session *ss = reinterpret_cast<view_session*>(session.get());
            if (ss) {
                ss->on_view_activation(id);
            }
        }
    }

    void view_server::call_deactivation_listener(const ui::view::view_id id) {
        for (auto &[uid, session]: sessions) {
            view_session *ss = reinterpret_cast<view_session*>(session.get());
            if (ss) {
                ss->on_view_deactivation(id);
            }
        }
    }

    void view_server::make_view_active(view_session *activator, const ui::view::view_id &active_id, const ui::view::custom_message &msg) {
        view_session *deactivate_ss = active_view_session();
        std::optional<ui::view::view_id> old_id = std::nullopt;

        if (deactivate_ss) {
            old_id = deactivate_ss->active_view();
            deactivate_ss->queue_event({
                ui::view::view_event::event_deactive_view, old_id.value(), active_id, 0, 0 });
            deactivate_ss->clear_active_view();

            call_deactivation_listener(old_id.value());
        } else {
            old_id = ui::view::view_id{ 0, 0 };
        }

        activator->set_active_view(active_id);
        activator->queue_event({ ui::view::view_event::event_active_view, active_id, old_id.value(),
            msg.id_, static_cast<std::int32_t>(msg.data_.size()) }, msg);

        call_activation_listener(active_id);
    }

    void view_server::deactivate_active_view() {
        view_session *deactivate_ss = active_view_session();

        if (deactivate_ss) {
            std::optional<ui::view::view_id> old_id = deactivate_ss->active_view();
            deactivate_ss->queue_event({
                ui::view::view_event::event_deactive_view, old_id.value(), ui::view::EMPTY_VIEW_ID, 0, 0 });
            deactivate_ss->clear_active_view();

            call_deactivation_listener(old_id.value());
        }
    }

    view_session::view_session(service::typical_server *server, const kernel::uid session_uid, epoc::version client_version)
        : service::typical_session(server, session_uid, client_version)
        , outstanding_activation_notify_(false)
        , outstanding_deactivation_notify_(false)
        , next_activation_id_(ui::view::EMPTY_VIEW_ID)
        , next_deactivation_id_(ui::view::EMPTY_VIEW_ID)
        , app_uid_(0)
        , active_view_id_index_(-1) {
    }

    std::optional<ui::view::view_id> view_session::active_view() {
        if (active_view_id_index_ < 0) {
            return std::nullopt;
        }

        return ids_[active_view_id_index_];
    }

    void view_session::set_active_view(const ui::view::view_id &id) {
        auto iterator = std::find(ids_.begin(), ids_.end(), id);
        if (iterator != ids_.end()) {
            active_view_id_index_ = static_cast<std::int32_t>(std::distance(ids_.begin(), iterator));
        }
    }

    void view_session::clear_active_view() {
        active_view_id_index_ = -1;
    }

    void view_session::on_view_activation(const ui::view::view_id &id) {
        if (outstanding_activation_notify_) {
            if ((next_activation_id_ == ui::view::EMPTY_VIEW_ID) || (next_activation_id_ == id)) {
                queue_.queue_event({ ui::view::view_event::event_active_notification, id, ui::view::EMPTY_VIEW_ID, 0, 0 });
            }
        }
    }

    void view_session::on_view_deactivation(const ui::view::view_id &id) {
        if (outstanding_deactivation_notify_) {
            if ((next_deactivation_id_ == ui::view::EMPTY_VIEW_ID) || (next_deactivation_id_ == id)) {
                queue_.queue_event({ ui::view::view_event::event_deactive_notification, id, ui::view::EMPTY_VIEW_ID, 0, 0 });
            }
        }
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

        ids_.push_back(id.value());
        ctx->complete(epoc::error_none);
    }

    void view_session::remove_view(service::ipc_context *ctx) {
        std::optional<ui::view::view_id> id = ctx->get_argument_data_from_descriptor<ui::view::view_id>(0);

        if (!id) {
            ctx->complete(epoc::error_argument);
            return;
        }

        auto iterator = std::find(ids_.begin(), ids_.end(), id.value());
        if (iterator == ids_.end()) {
            LOG_ERROR(SERVICE_UI, "View ID can not be found!");
            ctx->complete(epoc::error_not_found);

            return;
        }

        const std::int32_t pos_of_del = static_cast<std::int32_t>(std::distance(ids_.begin(), iterator));
        if (pos_of_del == active_view_id_index_) {
            LOG_ERROR(SERVICE_UI, "Can't remove active view!");
            ctx->complete(epoc::error_in_use);

            return;
        } else if (pos_of_del < active_view_id_index_) {
            active_view_id_index_--;
        }

        ids_.erase(iterator);
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

    void view_session::request_view_event_cancel(service::ipc_context *ctx) {
        queue_.cancel();
        ctx->complete(epoc::error_none);
    }

    void view_session::active_view(service::ipc_context *ctx, const bool should_complete) {
        kernel_system *kern = server<view_server>()->get_kernel_object_owner();

        std::optional<epoc::uid> custom_message_id;
        std::optional<ui::view::view_id> id;
        std::uint8_t *custom_message_buf = nullptr;
        std::size_t custom_message_size = 0;

        if (kern->get_epoc_version() < epocver::epoc81a) {
            struct active_view_fundamental_info {
                ui::view::view_id app_id_;
                epoc::uid custom_message_id_;
                std::uint32_t custom_message_size_;
            };

            std::optional<active_view_fundamental_info> info = ctx->get_argument_data_from_descriptor<active_view_fundamental_info>(0);
            if (!info) {
                ctx->complete(epoc::error_argument);
                return;
            }

            id = info->app_id_;
            custom_message_id = info->custom_message_id_;
            custom_message_size = info->custom_message_size_;
            custom_message_buf = ctx->get_descriptor_argument_ptr(1);
        } else {
            custom_message_id = ctx->get_argument_value<epoc::uid>(1);
            custom_message_buf = ctx->get_descriptor_argument_ptr(2);
            custom_message_size = ctx->get_argument_data_size(2);
            id = ctx->get_argument_data_from_descriptor<ui::view::view_id>(0);
        }

        if (!id || !custom_message_id) {
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

        auto iterator = std::find(ids_.begin(), ids_.end(), id.value());
        if (iterator == ids_.end()) {
            LOG_ERROR(SERVICE_UI, "View ID can not be found!");
            ctx->complete(epoc::error_not_found);

            return;
        }

        server<view_server>()->make_view_active(this, id.value(), { custom_message_buf_cop, custom_message_id.value() });
        ctx->complete(epoc::error_none);
    }

    void view_session::deactive_view(service::ipc_context *ctx, const bool should_complete) {
        view_server *svr = server<view_server>();
        svr->deactivate_active_view();

        ctx->complete(epoc::error_none);
    }

    void view_session::async_message_for_client_to_panic_with(service::ipc_context *ctx) {
        to_panic_.sts = ctx->msg->request_sts;
        to_panic_.requester = ctx->msg->own_thr;
    }

    void view_session::get_priority(service::ipc_context *ctx) {
        const std::uint32_t priority = server<view_server>()->priority();
        ctx->write_data_to_descriptor_argument(0, &priority);
        ctx->complete(epoc::error_none);
    }

    void view_session::get_custom_message(service::ipc_context *ctx) {
        const ui::view::custom_message custom = queue_.current_custom_message();
        ctx->write_data_to_descriptor_argument(0, custom.data_.data(), static_cast<std::uint32_t>(custom.data_.size()), nullptr, true);
        ctx->complete(epoc::error_none);
    }

    void view_session::fetch(service::ipc_context *ctx) {
        kernel_system *kern = server<view_server>()->get_kernel_object_owner();

        if (!kern->is_eka1() && (ctx->msg->function >= view_opcode_set_system_default_view) && (ctx->msg->function < view_opcode_end_nocap)) {
            // This is moved for capability safety in EKA2...
            ctx->msg->function += 1;
        }

        if (!kern->is_eka1() && (kern->get_epoc_version() <= epocver::epoc95)) {
            // These are swapped orders
            if (ctx->msg->function == view_opcode_deactivate_active_view_if_owner_match) {
                ctx->msg->function = view_opcode_set_background_color;
            } else if ((ctx->msg->function >= view_opcode_set_protected) && (ctx->msg->function <= view_opcode_set_cross_check_uid)) {
                // There is no set protected here, so raise it up
                ctx->msg->function += 1;
            }
        }

        if (kern->is_eka1() && (kern->get_epoc_version() >= epocver::epoc81a)) {
            // Unknown opcode 2, and start app does not exist, so push it down
            if (ctx->msg->function <= view_opcode_start_app) {
                ctx->msg->function -= 1;
            }
        }

        switch (ctx->msg->function) {
        case view_opcode_async_msg_for_client_to_panic: {
            async_message_for_client_to_panic_with(ctx);
            break;
        }

        case view_opcode_priority:
        case view_opcode_priority_mirror:
        case view_opcode_priority_mirror2: {
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

        case view_opcode_remove_view:
            remove_view(ctx);
            break;

        case view_opcode_active_view: {
            active_view(ctx, true);
            break;
        }

        // TODO: Really check if owner matches
        case view_opcode_deactivate_active_view_if_owner_match:
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

        case view_opcode_request_custom_message:
            get_custom_message(ctx);
            break;

        case view_opcode_set_background_color: {
            LOG_WARN(SERVICE_UI, "SetBackgroundColor stubbed");
            ctx->complete(epoc::error_none);
            break;
        }

        case view_opcode_request_view_event_cancel:
            request_view_event_cancel(ctx);
            break;

        default:
            LOG_ERROR(SERVICE_UI, "Unimplemented view session opcode {}", ctx->msg->function);
            break;
        }
    }
}