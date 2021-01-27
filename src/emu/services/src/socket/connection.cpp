/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/socket/connection.h>
#include <services/socket/socket.h>
#include <services/socket/server.h>

#include <common/log.h>

namespace eka2l1::epoc::socket {
    socket_connection_proxy::socket_connection_proxy(socket_client_session *parent, connection *conn)
        : socket_subsession(parent)
        , conn_(conn) {
    }

    void socket_connection_proxy::dispatch(service::ipc_context *ctx) {
        if (parent_->is_oldarch()) {
            switch (ctx->msg->function) {
            default:
                LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                break;
            }
        } else {
            switch (ctx->msg->function) {
            default:
                LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                break;
            }
        }
    }
}