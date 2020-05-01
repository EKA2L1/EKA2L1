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
#include <epoc/services/msv/msv.h>

#include <epoc/utils/err.h>

namespace eka2l1 {
    msv_server::msv_server(eka2l1::system *sys)
        : service::typical_server(sys, "!MsvServer") {
    }

    void msv_server::connect(service::ipc_context &context) {
        create_session<msv_client_session>(&context);
        context.set_request_status(epoc::error_none);
    }

    msv_client_session::msv_client_session(service::typical_server *serv, const std::uint32_t ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void msv_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case msv_notify_session_event: {
            notify_session_event(ctx);
            break;
        }

        default: {
            LOG_ERROR("Unimplemented opcode for MsvServer 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    void msv_client_session::notify_session_event(service::ipc_context *ctx) {
        if (msv_info.empty()) {
            msv_info = epoc::notify_info{ ctx->msg->request_sts, ctx->msg->own_thr };
        }
    }
}
