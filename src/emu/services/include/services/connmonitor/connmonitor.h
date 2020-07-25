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

    enum connmonitor_opcode {
        connmonitor_get_connection_count = 0,
        connmonitor_get_connection_info = 1,
        connmonitor_get_sub_connection_info = 2,
        connmonitor_get_int_attribute = 3,
        connmonitor_get_uint_attribute = 4,
        connmonitor_get_bool_attribute = 5,
        connmonitor_get_string_attribute = 6,
        connmonitor_get_pckg_attribute = 7,
        connmonitor_set_int_attribute = 8,
        connmonitor_set_uint_attribute = 9,
        connmonitor_set_bool_attribute = 10,
        connmonitor_set_string_attribute = 11,
        connmonitor_set_pckg_attribute = 12,
        connmonitor_cancel_async_request = 13,
        connmonitor_receive_event = 14,
        connmonitor_cancel_receive_event = 15
    };

    enum bearer_id {
        bearer_id_all = 0,
        bearer_id_gprs = 2000000,
        bearer_id_wcdma = 2000001,
        bearer_id_csd = 2000002,
        bearer_id_gsm = 2000003,
        bearer_id_wlan = 2000004,
        bearer_id_lan = 2000005,
        bearer_id_wcdmacsd = 2000006,
        bearer_id_cdma2000 = 2000007,
        bearer_id_virtualvpn = 2000008
    };

    class connmonitor_server : public service::typical_server {
    public:
        explicit connmonitor_server(eka2l1::system *sys);

        void connect(service::ipc_context &context) override;
    };

    struct connmonitor_client_session : public service::typical_session {
        epoc::notify_info nof_info;

        explicit connmonitor_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void get_connection_count(eka2l1::service::ipc_context *ctx);
        void receive_event(eka2l1::service::ipc_context *ctx);
        void get_int_attribute(eka2l1::service::ipc_context *ctx);
        void get_uint_attribute(eka2l1::service::ipc_context *ctx);
        void get_connection_info(eka2l1::service::ipc_context *ctx);
        void cancel_async_request(eka2l1::service::ipc_context *ctx);
        void cancel_receive_event(eka2l1::service::ipc_context *ctx);
    };
}
