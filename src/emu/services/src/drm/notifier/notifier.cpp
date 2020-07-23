/*
 * Copyright (c) 2020 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#include <epoc/epoc.h>
#include <services/drm/notifier/notifier.h>

#include <utils/err.h>

namespace eka2l1 {
    drm_notifier_server::drm_notifier_server(eka2l1::system *sys)
        : service::typical_server(sys, "!DRMNotifier")
        , storage_(nullptr) {
    }

    void drm_notifier_server::connect(service::ipc_context &context) {
        create_session<drm_notifier_client_session>(&context);
        context.complete(epoc::error_none);
    }

    drm_notifier_client_session::drm_notifier_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void drm_notifier_client_session::send_event(service::ipc_context *ctx) {
        std::optional<address> data_to_send_ptr = ctx->get_argument_value<address>(0);
        std::optional<std::uint32_t> event_type_op = ctx->get_argument_value<std::uint32_t>(1);

        if (!data_to_send_ptr || !event_type_op) {
            ctx->complete(epoc::error_argument);
            return;
        }
        
        kernel::process *pr = ctx->msg->own_thr->owning_process();
        std::uint8_t *data_to_send = eka2l1::ptr<std::uint8_t>(data_to_send_ptr.value()).get(pr);
        
        if (!data_to_send || !server<drm_notifier_server>()->send_event(event_type_op.value(), data_to_send)) {
            ctx->complete(epoc::error_argument);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void drm_notifier_client_session::listen_for_event(service::ipc_context *ctx) {
        std::optional<address> data_to_write_guest_ptr = ctx->get_argument_value<address>(0);
        std::optional<address> event_type_to_write_guest_ptr = ctx->get_argument_value<address>(1);

        if (!data_to_write_guest_ptr || !event_type_to_write_guest_ptr) {
            ctx->complete(epoc::error_argument);
            return;
        }
        
        kernel::process *pr = ctx->msg->own_thr->owning_process();

        std::uint8_t *data_to_write = eka2l1::ptr<std::uint8_t>(data_to_write_guest_ptr.value()).get(pr);
        epoc::des8 *event_type_to_write = eka2l1::ptr<epoc::des8>(event_type_to_write_guest_ptr.value()).get(pr);

        if (!data_to_write || !event_type_to_write) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::notify_info info;
        info.sts = ctx->msg->request_sts;
        info.requester = ctx->msg->own_thr;

        if (!listen(info, data_to_write, event_type_to_write)) {
            ctx->complete(epoc::error_in_use);
            return;
        }
    }

    void drm_notifier_client_session::cancel_listen_for_event(service::ipc_context *ctx) {
        if (!notify_.empty()) {
            notify_.complete(epoc::error_cancel);
        }

        ctx->complete(epoc::error_none);
    }

    void drm_notifier_client_session::register_event(service::ipc_context *ctx) {
        std::optional<std::uint32_t> event_type = ctx->get_argument_data_from_descriptor<std::uint32_t>(0);

        if (!event_type) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (!register_event(event_type.value(), "")) {
            ctx->complete(epoc::error_already_exists);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void drm_notifier_client_session::unregister_event(service::ipc_context *ctx) {
        std::optional<std::uint32_t> event_type = ctx->get_argument_data_from_descriptor<std::uint32_t>(0);

        if (!event_type) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (!unregister_event(event_type.value(), "")) {
            ctx->complete(epoc::error_already_exists);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void drm_notifier_client_session::register_event_with_uri(service::ipc_context *ctx) {
        std::optional<std::uint32_t> event_type = ctx->get_argument_data_from_descriptor<std::uint32_t>(0);
        std::optional<std::string> uri = ctx->get_argument_value<std::string>(1);

        if (!event_type || !uri) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (!register_event(event_type.value(), uri.value())) {
            ctx->complete(epoc::error_already_exists);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void drm_notifier_client_session::unregister_event_with_uri(service::ipc_context *ctx) {
        std::optional<std::uint32_t> event_type = ctx->get_argument_data_from_descriptor<std::uint32_t>(0);
        std::optional<std::string> uri = ctx->get_argument_value<std::string>(1);

        if (!event_type || !uri) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (!unregister_event(event_type.value(), uri.value())) {
            ctx->complete(epoc::error_already_exists);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void drm_notifier_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case drm_notifier_send_events:
            send_event(ctx);
            break;

        case drm_notifier_listen_for_event:
            listen_for_event(ctx);
            break;

        case drm_notifier_cancel_listen_for_event:
            cancel_listen_for_event(ctx);
            break;

        case drm_notifier_register_accept_event:
            register_event(ctx);
            break;

        case drm_notifier_register_accept_event_with_uri:
            register_event_with_uri(ctx);
            break;

        case drm_notifier_unregister_accept_event:
            unregister_event(ctx);
            break;

        case drm_notifier_unregister_accept_event_with_uri:
            unregister_event_with_uri(ctx);
            break;

        default:
            LOG_ERROR("Unimplemented DRMNotifier opcode 0x{:X}", ctx->msg->function);
            break;
        }
    }
}
