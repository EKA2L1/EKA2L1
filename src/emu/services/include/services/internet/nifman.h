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

#include <kernel/server.h>
#include <services/framework.h>

namespace eka2l1 {
    enum nifman_opcode {
        nifman_start = 0x0,
        nifman_open = 0x1,
        nifman_stop = 0x2,
        nifman_progress = 0x3,
        nifman_progress_notification = 0x4,
        nifman_cancel_progress_notification = 0x5,
        nifman_last_progress_error = 0x7,
        nifman_network_active = 0x8,
        nifman_set_overrides = 0x65,
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
        epoc::socket::conn_progress progress_{};
        std::unique_ptr<service::ipc_context> progress_notification_;

        void open(service::ipc_context *ctx);
        void set_progress(std::int32_t stage);
        void deliver_progress(service::ipc_context *ctx);
        void progress_notification(service::ipc_context *ctx);
        void get_active_settings(service::ipc_context *ctx, const epoc::socket::setting_type type);

    public:
        explicit nifman_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);
        ~nifman_client_session() override;
        void fetch(service::ipc_context *ctx) override;
    };
}
