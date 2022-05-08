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
#include <services/socket/server.h>
#include <services/socket/socket.h>

#include <common/log.h>
#include <utils/err.h>
#include <system/epoc.h>

namespace eka2l1::epoc::socket {
    connection::connection(protocol *pr, saddress dest)
        : pr_(pr)
        , sock_(nullptr)
        , dest_(dest) {
    }

    std::size_t connection::register_progress_advance_callback(progress_advance_callback cb) {
        return progress_callbacks_.add(cb);
    }

    bool connection::remove_progress_advance_callback(const std::size_t handle) {
        return progress_callbacks_.remove(handle);
    }

    socket_connection_proxy::socket_connection_proxy(socket_client_session *parent, connection *conn)
        : socket_subsession(parent)
        , conn_(conn) {
    }

    void socket_connection_proxy::dispatch(service::ipc_context *ctx) {
        if (parent_->is_oldarch()) {
            switch (ctx->msg->function) {
            default:
                LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                ctx->complete(epoc::error_none);

                break;
            }
        } else {
            if (ctx->sys->get_symbian_version_use() >= epocver::epoc95) {
                switch (ctx->msg->function) {
                default:
                    LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                    ctx->complete(epoc::error_none);

                    break;
                }
            } else {
                switch (ctx->msg->function) {
                case socket_cm_api_ext_interface_send_receive:
                    // Async, but we should complete it in sometimes
                    // Complete with not right result will create stuck or crash sometimes
                    break;

                default:
                    LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket connection opcode: {}", ctx->msg->function);
                    ctx->complete(epoc::error_none);

                    break;
                }
            }
        }
    }
}