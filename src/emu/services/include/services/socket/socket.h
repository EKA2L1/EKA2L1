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

#pragma once

#include <services/framework.h>
#include <kernel/server.h>

namespace eka2l1 {

    enum socket_opcode {
        socket_sr_get_by_number = 0x3F,
        socket_cn_get_long_des_setting = 0x51
    };

    class socket_server : public service::typical_server {
    public:
        explicit socket_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct socket_client_session : public service::typical_session {
        explicit socket_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void sr_get_by_number(eka2l1::service::ipc_context *ctx);
        void cn_get_long_des_setting(eka2l1::service::ipc_context *ctx);
    };
}
