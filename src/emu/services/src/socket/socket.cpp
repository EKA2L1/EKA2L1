/*
 * Copyright (c) 2021 EKA2L1 Team
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

#include <services/socket/socket.h>
#include <services/socket/server.h>

#include <utils/err.h>

namespace eka2l1::epoc::socket {
    socket_socket::socket_socket(socket_client_session *parent, std::unique_ptr<socket> &sock)
        : socket_subsession(parent)
        , sock_(std::move(sock)) {
    }

    void socket_socket::get_option(service::ipc_context *ctx) {
        std::optional<std::uint32_t> option_name = ctx->get_argument_value<std::uint32_t>(0);
        std::optional<std::uint32_t> option_fam = ctx->get_argument_value<std::uint32_t>(2);

        if (!option_name || !option_fam) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::uint8_t *dest_buffer = ctx->get_descriptor_argument_ptr(1);
        std::size_t max_size = ctx->get_argument_max_data_size(1);

        if (!dest_buffer) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::size_t res = sock_->get_option(option_name.value(), option_fam.value(), dest_buffer, max_size);
        if (res == static_cast<std::size_t>(-1)) {
            LOG_ERROR(SERVICE_ESOCK, "Fail to get value of socket option!");
            ctx->complete(epoc::error_general);

            return;
        }

        ctx->set_descriptor_argument_length(1, static_cast<std::uint32_t>(res));
        ctx->complete(epoc::error_none);
    }
    
    void socket_socket::close(service::ipc_context *ctx) {
        parent_->subsessions_.remove(id_);
        ctx->complete(epoc::error_none);
    }
    
    void socket_socket::dispatch(service::ipc_context *ctx) {
        if (parent_->is_oldarch()) {
            switch (ctx->msg->function) {
            default:
                break;
            }
        } else {
            switch (ctx->msg->function) {
            case socket_so_get_opt:
                get_option(ctx);
                return;

            case socket_so_close:
                close(ctx);
                break;
                
            default:
                break;
            }
        }

        LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket opcode: {}", ctx->msg->function);
    }
}