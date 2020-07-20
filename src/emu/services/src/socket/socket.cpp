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
#include <services/socket/socket.h>

#include <utils/err.h>

namespace eka2l1 {
    socket_server::socket_server(eka2l1::system *sys)
        : service::typical_server(sys, "!SocketServer") {
    }

    void socket_server::connect(service::ipc_context &context) {
        create_session<socket_client_session>(&context);
        context.complete(epoc::error_none);
    }

    socket_client_session::socket_client_session(service::typical_server *serv, const std::uint32_t ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    void socket_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case socket_sr_get_by_number:
            sr_get_by_number(ctx);
            break;

        case socket_cn_get_long_des_setting:
            cn_get_long_des_setting(ctx);
            break;

        default: {
            LOG_ERROR("Unimplemented opcode for Socket server 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    void socket_client_session::sr_get_by_number(eka2l1::service::ipc_context *ctx) {
        std::int32_t port = *(ctx->get_argument_value<std::int32_t>(1));

        std::string name = "DummyService";
        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&name[0]),
            static_cast<std::uint32_t>(name.length()));
        ctx->complete(epoc::error_none);
    }
    
    void socket_client_session::cn_get_long_des_setting(eka2l1::service::ipc_context *ctx) {
        LOG_TRACE("CnGetLongDesSetting stubbed");
        ctx->complete(epoc::error_none);
    }
}
