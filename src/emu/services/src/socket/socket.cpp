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
#include <services/socket/connection.h>
#include <services/socket/resolver.h>
#include <services/socket/socket.h>

#include <utils/err.h>

namespace eka2l1 {
    std::string get_socket_server_name_by_epocver(const epocver ver) {
        if (ver <= epocver::eka2) {
            return "SocketServer";
        }

        return "!SocketServer";
    }

    socket_server::socket_server(eka2l1::system *sys)
        : service::typical_server(sys, get_socket_server_name_by_epocver((sys->get_symbian_version_use()))) {
    }

    void socket_server::connect(service::ipc_context &context) {
        create_session<socket_client_session>(&context);
        context.complete(epoc::error_none);
    }

    epoc::socket::host &socket_server::host_by_info(const std::uint32_t family, const std::uint32_t protocol) {
        std::uint64_t the_id = static_cast<std::uint64_t>(protocol) | (static_cast<std::uint64_t>(family) << 32);
        if (hosts_.find(the_id) == hosts_.end()) {
            hosts_[the_id].name_ = u"eka2l1";
        }

        return hosts_[the_id];
    }

    socket_client_session::socket_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    bool socket_client_session::is_oldarch() {
        return server<socket_server>()->get_kernel_object_owner()->is_eka1();
    }

    void socket_client_session::fetch(service::ipc_context *ctx) {
        if (is_oldarch()) {
            switch (ctx->msg->function) {
            case socket_old_hr_open:
                hr_create(ctx, false);
                return;

            default:
                break;
            }
        } else {
            switch (ctx->msg->function) {
            case socket_pr_find:
                pr_find(ctx);
                return;

            case socket_sr_get_by_number:
                sr_get_by_number(ctx);
                return;

            case socket_cn_get_long_des_setting:
                cn_get_long_des_setting(ctx);
                return;

            default:
                break;
            }
        }

        std::optional<std::uint32_t> subsess_id = ctx->get_argument_value<std::uint32_t>(3);

        if (subsess_id && (subsess_id.value() > 0)) {
            socket_subsession_instance *inst = subsessions_.get(subsess_id.value());

            if (inst) {
                inst->get()->dispatch(ctx);
                return;
            }
        }
    
        LOG_ERROR("Unimplemented opcode for Socket server 0x{:X}", ctx->msg->function);
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

    void socket_client_session::pr_find(service::ipc_context *ctx) {
        LOG_TRACE("Protocol find stubbed with not found!");
        ctx->complete(epoc::error_not_found);
    }

    void socket_client_session::hr_create(service::ipc_context *ctx, const bool with_conn) {
        std::optional<std::uint32_t> addr_family = ctx->get_argument_value<std::uint32_t>(0);
        std::optional<std::uint32_t> protocol = ctx->get_argument_value<std::uint32_t>(1);
        std::optional<std::uint32_t> conn_subhandle = ctx->get_argument_value<std::uint32_t>(2);

        if (!addr_family || !protocol || (with_conn && !conn_subhandle)) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::socket::socket_connection_proxy *conn = nullptr;

        if (with_conn) {
            socket_subsession_instance *inst = subsessions_.get(conn_subhandle.value());

            if (!inst) {
                ctx->complete(epoc::error_argument);
                return;
            }

            conn = reinterpret_cast<epoc::socket::socket_connection_proxy*>(inst->get());
        }

        // Create new session
        socket_subsession_instance hr_inst = std::make_unique<epoc::socket::socket_host_resolver>(
            this, addr_family.value(), protocol.value(), conn ? conn->get_connection() : nullptr);

        const std::uint32_t id = static_cast<std::uint32_t>(subsessions_.add(hr_inst));
        subsessions_.get(id)->get()->set_id(id);

        // Write the subsession handle
        ctx->write_data_to_descriptor_argument<std::uint32_t>(3, id);
        ctx->complete(epoc::error_none);
    }
}
