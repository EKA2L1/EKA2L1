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

#include <services/socket/host.h>
#include <services/socket/server.h>

#include <utils/err.h>
#include <system/epoc.h>

namespace eka2l1::epoc::socket {
    socket_host_resolver::socket_host_resolver(socket_client_session *parent, std::unique_ptr<host_resolver> &resolver,
        connection *conn)
        : socket_subsession(parent)
        , resolver_(std::move(resolver))
        , conn_(conn) {
    }

    void socket_host_resolver::get_host_name(service::ipc_context *ctx) {
        ctx->write_arg(0, resolver_->host_name());
        ctx->complete(epoc::error_none);
    }

    void socket_host_resolver::set_host_name(service::ipc_context *ctx) {
        std::optional<std::u16string> new_name = ctx->get_argument_value<std::u16string>(0);

        if (!new_name) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if (!resolver_->host_name(new_name.value())) {
            LOG_ERROR(SERVICE_ESOCK, "Failed to set host name!");
            ctx->complete(epoc::error_general);

            return;
        }

        ctx->complete(epoc::error_none);
    }

    void socket_host_resolver::get_by_name(service::ipc_context *ctx) {
        std::optional<std::u16string> name = ctx->get_argument_value<std::u16string>(0);

        if (!name.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::socket::name_entry entry_holder;
        entry_holder.name_ = name.value();

        if (!resolver_->get_by_name(entry_holder)) {
            ctx->complete(epoc::error_general);
            return;
        }

        ctx->write_data_to_descriptor_argument(1, entry_holder);
        ctx->complete(epoc::error_none);
    }
    
    void socket_host_resolver::next(service::ipc_context *ctx) {
        epoc::socket::name_entry entry_holder;

        if (!resolver_->next(entry_holder)) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->write_data_to_descriptor_argument(1, entry_holder);
        ctx->complete(epoc::error_none);
    }

    void socket_host_resolver::close(service::ipc_context *ctx) {
        parent_->subsessions_.remove(id_);
        ctx->complete(epoc::error_none);
    }

    void socket_host_resolver::dispatch(service::ipc_context *ctx) {
        if (parent_->is_oldarch()) {
            switch (ctx->msg->function) {
            case socket_old_hr_get_host_name:
                get_host_name(ctx);
                return;

            case socket_old_hr_set_host_name:
                set_host_name(ctx);
                return;

            case socket_old_hr_close:
                close(ctx);
                return;

            default:
                break;
            }
        } else {
            if (ctx->sys->get_symbian_version_use() >= epocver::epoc95) {
                switch (ctx->msg->function) {
                case socket_reform_hr_get_by_name:
                    get_by_name(ctx);
                    return;

                case socket_reform_hr_close:
                    close(ctx);
                    return;

                case socket_reform_hr_next:
                    next(ctx);
                    return;

                default:
                    break;
                }
            } else {
                switch (ctx->msg->function) {
                case socket_hr_get_by_name:
                    get_by_name(ctx);
                    return;

                case socket_hr_close:
                    close(ctx);
                    return;

                default:
                    break;
                }
            }
        }

        LOG_ERROR(SERVICE_ESOCK, "Unimplemented socket host resolver opcode: {}", ctx->msg->function);
    }
}