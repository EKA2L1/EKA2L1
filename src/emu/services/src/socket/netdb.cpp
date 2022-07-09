/*
 * Copyright (c) 2022 EKA2L1 Team
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

#include <services/socket/server.h>
#include <services/socket/netdb.h>

#include <utils/err.h>
#include <system/epoc.h>

namespace eka2l1::epoc::socket {
    socket_net_database::socket_net_database(socket_client_session *parent, std::unique_ptr<net_database> &net_db)
        : socket_subsession(parent)
        , net_db_(std::move(net_db)) {
    }

    void socket_net_database::query(service::ipc_context *ctx) {
        const char *query_data = reinterpret_cast<const char*>(ctx->get_descriptor_argument_ptr(0));
        epoc::des8 *result_des = eka2l1::ptr<epoc::des8>(ctx->msg->args.args[2]).get(ctx->msg->own_thr->owning_process());

        std::size_t query_data_size = ctx->get_argument_data_size(0);
        if (!query_data || !result_des || !query_data_size) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);
        net_db_->query(query_data, static_cast<std::uint32_t>(query_data_size), result_des, info);
    }

    void socket_net_database::cancel(service::ipc_context *ctx) {
        net_db_->cancel();
        ctx->complete(epoc::error_none);
    }

    void socket_net_database::close(service::ipc_context *ctx) {
        parent_->subsessions_.remove(id_);
        ctx->complete(epoc::error_none);
    }

    void socket_net_database::dispatch(service::ipc_context *ctx) {
        if (parent_->is_oldarch()) {
            switch (ctx->msg->function) {
            case socket_old_ndb_query:
                query(ctx);
                return;

            case socket_old_ndb_cancel:
                cancel(ctx);
                return;

            case socket_old_ndb_close:
                close(ctx);
                return;

            default:
                break;
            }
        }

        LOG_ERROR(SERVICE_ESOCK, "Unimplemented net database opcode: {}", ctx->msg->function);
    }
}