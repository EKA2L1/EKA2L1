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

#pragma once

#include <kernel/server.h>
#include <services/framework.h>

namespace eka2l1 {
    enum goom_monitor_opcode {
        goom_monitor_request_free_memory = 0,
        goom_monitor_memory_allocations_complete = 1,
        goom_monitor_cancel_request_free_memory = 2,
        goom_monitor_this_app_is_not_existing = 3,
        goom_monitor_request_optional_ram = 4
    };

    class goom_monitor_server : public service::typical_server {
    public:
        explicit goom_monitor_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct goom_monitor_session : public service::typical_session {
        explicit goom_monitor_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void request_free_memory(service::ipc_context *ctx);
    };
}
