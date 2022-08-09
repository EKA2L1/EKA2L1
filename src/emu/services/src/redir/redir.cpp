/*
 * Copyright (c) 2022 EKA2L1 Team
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

#include <services/redir/redir.h>
#include <system/epoc.h>

#include <utils/err.h>

namespace eka2l1 {
    redir_server::redir_server(eka2l1::system *sys)
        : service::typical_server(sys, "RedirServer") {
    }

    void redir_server::connect(service::ipc_context &context) {
        create_session<redir_client_session>(&context);
        context.complete(epoc::error_none);
    }

    redir_client_session::redir_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void redir_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case redir_read:
            LOG_INFO(SERVICE_REDIR, "Stdin is not implemented with redirect server!");
            break;

        case redir_write: {
            std::optional<std::string> outstr = ctx->get_argument_value<std::string>(0);
            if (outstr.has_value()) {
                LOG_INFO(EMULATED_STDOUT, "{}", outstr.value());
            }
            break;
        }

        default:
            break;
        }

        ctx->complete(epoc::error_none);
    }
}
