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

namespace eka2l1 {
    enum alarm_opcode {
        alarm_get_alarm_id_list_by_state = 11,
        alarm_notify_change = 19,
        alarm_notify_change_cancel = 20,
        alarm_fetch_transfer_buffer = 21
    };

    enum alarm_state {
        alarm_state_in_preparation = -1,
        alarm_state_queued = 0,
        alarm_state_snoozed,
        alarm_state_waiting_to_notify,
        alarm_state_notifying,
        alarm_state_notified
    };

    class alarm_server : public service::typical_server {
    public:
        explicit alarm_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct alarm_session : public service::typical_session {
        std::vector<std::uint8_t> transfer_buf;
        std::vector<std::int32_t> alarm_ids;

        epoc::notify_info change_notify_info;
        std::uint8_t *change_alarm_id_fill;

    public:
        explicit alarm_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void get_alarm_id_list_by_state(service::ipc_context *ctx);
        void fetch_transfer_buffer(service::ipc_context *ctx);
        void notify_change(service::ipc_context *ctx);
        void notify_change_cancel(service::ipc_context *ctx);
    };
}
