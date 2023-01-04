/*
 * Copyright (c) 2023 EKA2L1 Team
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

#include <services/internet/browser.h>
#include <common/applauncher.h>
#include <common/log.h>
#include <utils/err.h>

namespace eka2l1 {
    browser_for_app_session::browser_for_app_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_version)
        : app_ui_based_session(serv, client_ss_uid, client_version) {

    }

    void browser_for_app_session::open_url(service::ipc_context *ctx) {
        // Unknown first 16 bytes, skip them and get to URL part
        std::optional<std::string> open_url_params = ctx->get_argument_value<std::string>(0);
        if (!open_url_params.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::string url = common::ucs2_to_utf8(std::u16string(reinterpret_cast<char16_t*>(open_url_params->substr(16).data()),
            (open_url_params->length() - 16) / 2));

        if (!common::launch_browser(url)) {
            LOG_ERROR(SERVICE_INTERNET, "Unable to open the browser for the URL: {}", url);
            ctx->complete(epoc::error_general);

            return;
        }

        // NOTE: Complete this with error none would trigger the read web browser state opcode. Indicate that the browser has been closed.
    }

    void browser_for_app_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case browser_for_app_open_url:
            open_url(ctx);
            return;

        default:
            break;
        }

        app_ui_based_session::fetch(ctx);
    }

    browser_for_app_server::browser_for_app_server(system *sys, std::uint32_t server_differentiator)
        : app_ui_based_server(sys, server_differentiator, 0x10008D39) {
    }

    void browser_for_app_server::connect(service::ipc_context &ctx) {
        create_session<browser_for_app_session>(&ctx);
        typical_server::connect(ctx);
    }
}