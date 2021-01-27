/*
 * Copyright (c) 2021 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
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

#pragma once

#include <services/socket/connection.h>

#include <services/framework.h>
#include <kernel/server.h>

namespace eka2l1 {
    enum nifman_opcode {
        nifman_open = 0x1,
        nifman_get_active_int_setting = 0x6D
    };

    class socket_server;
    
    class nifman_server : public service::typical_server {
    protected:
        socket_server *sock_serv_;

    public:
        explicit nifman_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;

        socket_server *get_socket_server() {
            return sock_serv_;
        }
    };

    struct nifman_client_session : public service::typical_session {
    private:
        epoc::socket::connect_agent *agent_;
        std::unique_ptr<epoc::socket::connection> conn_;

        void open(service::ipc_context *ctx);
        void get_active_settings(service::ipc_context *ctx, const epoc::socket::setting_type type);

    public:
        explicit nifman_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);
        void fetch(service::ipc_context *ctx) override;
    };
}
