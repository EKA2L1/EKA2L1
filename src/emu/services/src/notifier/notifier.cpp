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

#include <system/epoc.h>
#include <services/notifier/notifier.h>
#include <services/notifier/queries.h>

#include <utils/consts.h>
#include <utils/err.h>

#include <common/cvt.h>

namespace eka2l1 {
    std::string get_notifier_server_name_by_epocver(const epocver ver) {
        if (ver <= epocver::eka2) {
            return "Notifier";
        }

        return "!Notifier";
    }

    notifier_server::notifier_server(eka2l1::system *sys)
        : service::typical_server(sys, get_notifier_server_name_by_epocver(sys->get_symbian_version_use())) {
        epoc::notifier::add_builtin_plugins(kern, plugins_);
    }

    epoc::notifier::plugin_base *notifier_server::get_plugin(const epoc::uid id) {
        auto result = std::lower_bound(plugins_.begin(), plugins_.end(), id, [](const epoc::notifier::plugin_instance &lhs, const epoc::uid rhs) {
            return lhs->unique_id() < rhs;
        });

        if ((result == plugins_.end()) || ((*result)->unique_id() != id)) {
            return nullptr;
        }

        return result->get();
    }

    void notifier_server::connect(service::ipc_context &context) {
        create_session<notifier_client_session>(&context);
        context.complete(epoc::error_none);
    }

    notifier_client_session::notifier_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void notifier_client_session::start_notifier(service::ipc_context *ctx) {
        std::optional<epoc::uid> plugin_uid = ctx->get_argument_value<epoc::uid>(0);
        if (!plugin_uid) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::notifier::plugin_base *plug = server<notifier_server>()->get_plugin(plugin_uid.value());
        if (!plug) {
            LOG_ERROR(ESERVICE_NOTIFIER, "Can't find the plugin with UID 0x{:X}. This is fine (but take note).", plugin_uid.value());
            ctx->complete(epoc::error_none);

            return;
        }
        
        kernel::process *caller_pr = ctx->msg->own_thr->owning_process();

        epoc::desc8 *request_data = eka2l1::ptr<epoc::desc8>(ctx->msg->args.args[1]).get(caller_pr);
        epoc::des8 *respond_data = eka2l1::ptr<epoc::des8>(ctx->msg->args.args[2]).get(caller_pr);

        // no respond is ok. but request must
        if (!request_data) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::notify_info complete_info;
        complete_info.sts = ctx->msg->request_sts;
        complete_info.requester = ctx->msg->own_thr;

        plug->handle(request_data, respond_data, complete_info);
    }
    
    void notifier_client_session::info_print(service::ipc_context *ctx) {
        std::optional<std::u16string> to_display = ctx->get_argument_value<std::u16string>(0);

        if (!to_display.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        // TODO: Add dialog to display this string
        LOG_INFO(ESERVICE_NOTIFIER, "Trying to display: {}", common::ucs2_to_utf8(to_display.value()));
        ctx->complete(epoc::error_none);
    }

    void notifier_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case notifier_notify:
            LOG_TRACE(ESERVICE_NOTIFIER, "Notifier opcode: notify stubbed");
            ctx->complete(epoc::error_none);
            break;

        case notifier_info_print:
            info_print(ctx);
            break;

        case notifier_start:
            start_notifier(ctx);
            break;

        case notifier_cancel:
            ctx->complete(epoc::error_none);
            break;

        case notifier_start_from_dll:
        case notifier_start_from_dll_and_get_response:
            // From doc: This function has never been implemented on any Symbian OS version.
            // Safe to return not supported
            ctx->complete(epoc::error_not_supported);
            return;

        default:
            LOG_ERROR(ESERVICE_NOTIFIER, "Unimplemented opcode for Notifier server 0x{:X}", ctx->msg->function);
            break;
        }
    }
}
