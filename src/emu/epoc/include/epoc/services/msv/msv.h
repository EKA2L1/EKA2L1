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

#include <epoc/services/framework.h>
#include <epoc/services/server.h>

namespace eka2l1 {

    enum msv_opcode {
        msv_notify_session_event = 0xB
    };

    class msv_server : public service::typical_server {
    public:
        explicit msv_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct msv_client_session : public service::typical_session {
        epoc::notify_info msv_info;

        explicit msv_client_session(service::typical_server *serv, const std::uint32_t ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void notify_session_event(service::ipc_context *ctx);

    };
}
