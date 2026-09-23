/*
 * Copyright (c) 2026 EKA2L1 Team
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

#include <services/framework.h>
#include <utils/reqsts.h>

namespace eka2l1 {
    enum memory_manager_opcode {
        memory_manager_request_free_ram = 1,
        memory_manager_request_free_ram_cancel = 2,
        memory_manager_notify_low_ram = 3,
        memory_manager_notify_low_ram_cancel = 4,
        memory_manager_free_ram_available = 5
    };

    class memory_manager_session : public service::typical_session {
        epoc::notify_info low_ram_nof_;
        kernel::uid low_ram_requester_ = 0;
        void cancel_low_ram_notification();

    public:
        explicit memory_manager_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver);

        ~memory_manager_session() override;
        void fetch(service::ipc_context *context) override;
    };

    // Minimal UIQ RAM policy: accept reservations; no memory-pressure events are generated.
    class memory_manager_server : public service::typical_server {
    public:
        explicit memory_manager_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };
}
