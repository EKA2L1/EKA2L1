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
    std::size_t socket::get_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) {
        LOG_ERROR(SERVICE_BLUETOOTH, "Unhandled bluetooth option family {} (id {})", option_family, option_id);
        return 0;
    }

    bool socket::set_option(const std::uint32_t option_id, const std::uint32_t option_family,
        std::uint8_t *buffer, const std::size_t avail_size) { 
        LOG_ERROR(SERVICE_BLUETOOTH, "Unhandled bluetooth option family {} (id {})", option_family, option_id);
        return false;
    }

    socket_socket::socket_socket(socket_client_session *parent, std::unique_ptr<socket> &sock)
        : socket_subsession(parent)
        , sock_(std::move(sock)) {
    }

    struct oldarch_option_description {
        std::uint32_t id_;
        eka2l1::ptr<epoc::desc8> data_;
        std::uint32_t data_size_;
    };

    void socket_socket::get_option(service::ipc_context *ctx) {
        std::optional<std::uint32_t> option_name;
        std::optional<std::uint32_t> option_fam;

        std::uint8_t *dest_buffer = nullptr;
        std::size_t max_size = 0;

        kernel::process *caller = ctx->msg->own_thr->owning_process();
        epoc::desc8 *custom_buf = nullptr;

        if (parent_->is_oldarch()) {
            auto desc_data = ctx->get_argument_data_from_descriptor<oldarch_option_description>(0);
            if (!desc_data.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            option_name = desc_data->id_;
            max_size = desc_data->data_size_;
            option_fam = ctx->get_argument_value<std::uint32_t>(1);

            epoc::desc8 *data_des_real = desc_data->data_.get(caller);
            if (!data_des_real) {
                ctx->complete(epoc::error_argument);
                return;
            }

            dest_buffer = reinterpret_cast<std::uint8_t*>(data_des_real->get_pointer(caller));
            custom_buf = data_des_real;
        } else {
            option_name = ctx->get_argument_value<std::uint32_t>(0);
            option_fam = ctx->get_argument_value<std::uint32_t>(2);

            dest_buffer = ctx->get_descriptor_argument_ptr(1);
            max_size = ctx->get_argument_max_data_size(1);
        }

        if (!option_name || !option_fam || !dest_buffer) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::size_t res = sock_->get_option(option_name.value(), option_fam.value(), dest_buffer, max_size);
        if (res == static_cast<std::size_t>(-1)) {
            LOG_ERROR(SERVICE_ESOCK, "Fail to get value of socket option!");
            ctx->complete(epoc::error_general);

            return;
        }

        if (custom_buf) {
            custom_buf->set_length(caller, static_cast<std::uint32_t>(res));
        } else {
            ctx->set_descriptor_argument_length(1, static_cast<std::uint32_t>(res));
        }

        ctx->complete(epoc::error_none);
    }

    void socket_socket::set_option(service::ipc_context *ctx) {
        std::optional<std::uint32_t> option_name;
        std::optional<std::uint32_t> option_fam;

        std::uint8_t *source_buffer = nullptr;
        std::size_t max_size = 0;

        kernel::process *caller = ctx->msg->own_thr->owning_process();

        if (parent_->is_oldarch()) {
            auto desc_data = ctx->get_argument_data_from_descriptor<oldarch_option_description>(0);
            if (!desc_data.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            option_name = desc_data->id_;
            max_size = desc_data->data_size_;
            option_fam = ctx->get_argument_value<std::uint32_t>(1);

            epoc::desc8 *data_des_real = desc_data->data_.get(caller);
            if (data_des_real) {
                source_buffer = reinterpret_cast<std::uint8_t*>(data_des_real->get_pointer(caller));
            }
        } else {
            option_name = ctx->get_argument_value<std::uint32_t>(0);
            option_fam = ctx->get_argument_value<std::uint32_t>(2);

            source_buffer = ctx->get_descriptor_argument_ptr(1);
            max_size = ctx->get_argument_max_data_size(1);
        }

        if (!option_name || !option_fam) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const bool res = sock_->set_option(option_name.value(), option_fam.value(), source_buffer, max_size);
        if (!res) {
            LOG_ERROR(SERVICE_ESOCK, "Fail to set value of socket option!");
            ctx->complete(epoc::error_general);

            return;
        }

        ctx->complete(epoc::error_none);
    }
    
    void socket_socket::close(service::ipc_context *ctx) {
        parent_->subsessions_.remove(id_);
        ctx->complete(epoc::error_none);
    }
    
    void socket_socket::dispatch(service::ipc_context *ctx) {
        if (parent_->is_oldarch()) {
            switch (ctx->msg->function) {
            case socket_old_so_set_opt:
                set_option(ctx);
                return;

            case socket_old_so_get_opt:
                get_option(ctx);
                return;

            case socket_old_so_close:
                close(ctx);
                return;

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
                return;
                
            default:
                break;
            }
        }

        LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket opcode: {}", ctx->msg->function);
    }
}