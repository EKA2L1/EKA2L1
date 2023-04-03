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

#include <services/notifier/notifier.h>
#include <services/notifier/queries.h>
#include <drivers/ui/input_dialog.h>
#include <system/epoc.h>

#include <utils/consts.h>
#include <utils/err.h>

#include <common/cvt.h>

namespace eka2l1 {
    std::string get_notifier_server_name_by_epocver(const epocver ver) {
        if (ver < epocver::epoc7) {
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
            LOG_ERROR(SERVICE_NOTIFIER, "Can't find the plugin with UID 0x{:X}. This is fine (but take note).", plugin_uid.value());
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
        LOG_INFO(SERVICE_NOTIFIER, "Trying to display: {}", common::ucs2_to_utf8(to_display.value()));
        ctx->complete(epoc::error_none);
    }

    void notifier_client_session::notify(service::ipc_context *ctx) {
        std::optional<std::uint32_t> length_text_line = ctx->get_argument_value<std::uint32_t>(2);
        std::optional<std::uint32_t> length_two_buttons = ctx->get_argument_value<std::uint32_t>(3);

        if (!length_text_line.has_value() || !length_two_buttons.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::uint16_t length_line1 = length_text_line.value() >> 16;
        std::uint16_t length_line2 = length_text_line.value() & 0xFFFF;

        std::uint16_t length_button_text1 = length_two_buttons.value() >> 16;
        std::uint16_t length_button_text2 = length_two_buttons.value() & 0xFFFF;

        std::optional<std::u16string> combined_text = ctx->get_argument_value<std::u16string>(1);
        if (!combined_text.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::u16string line1 = combined_text->substr(0, length_line1);
        std::u16string line2 = combined_text->substr(length_line1, length_line2);
        std::u16string button_text1 = combined_text->substr(length_line1 + length_line2, length_button_text1);
        std::u16string button_text2 = combined_text->substr(length_line1 + length_line2 + length_button_text1,
            length_button_text2);

        //LOG_TRACE(SERVICE_NOTIFIER, "Trying to display: {} {} {} {}", common::ucs2_to_utf8(line1),
        //    common::ucs2_to_utf8(line2), common::ucs2_to_utf8(button_text1), common::ucs2_to_utf8(button_text2));

        int *status = reinterpret_cast<int*>(ctx->get_descriptor_argument_ptr(0));
        if (!status) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::notify_info complete_info{ ctx->msg->request_sts, ctx->msg->own_thr };
        drivers::ui::show_yes_no_dialog(line1 + u'\n' + line2, button_text1, button_text2, [status, complete_info](int value) {
            *status = value;

            kernel_system *kern = complete_info.requester->get_kernel_object_owner();
            epoc::notify_info complete_info_copy = complete_info;

            kern->lock();
            complete_info_copy.complete(epoc::error_none);
            kern->unlock();
        });
    }

    void notifier_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case notifier_notify:
            notify(ctx);
            break;

        case notifier_info_print:
            info_print(ctx);
            break;

        case notifier_start:
        case notifier_update:
        case notfiier_start_and_get_response:
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

        case notifier_update_and_get_response:
            LOG_TRACE(SERVICE_NOTIFIER, "Update and get response is not implemented yet.");
            ctx->complete(epoc::error_none);
            return;

        default:
            LOG_ERROR(SERVICE_NOTIFIER, "Unimplemented opcode for Notifier server 0x{:X}", ctx->msg->function);
            break;
        }
    }
}
