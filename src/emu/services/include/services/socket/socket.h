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

#include <services/socket/common.h>
#include <services/socket/host.h>
#include <services/framework.h>
#include <common/container.h>
#include <kernel/server.h>

#include <map>
#include <memory>

namespace eka2l1 {
    namespace epoc::socket {
        class socket_host_resolver;
    }

    enum socket_opcode {
        socket_pr_find = 0x02,
        socket_hr_open = 0x28,
        socket_hr_open_with_connection = 0x3E,
        socket_sr_get_by_number = 0x3F,
        socket_cn_get_long_des_setting = 0x51
    };

    enum socket_opcode_old {
        socket_old_hr_open = 0x24,
        socket_old_hr_get_host_name = 0x28,
        socket_old_hr_set_host_name = 0x29,
        socket_old_hr_close = 0x2B
    };

    class socket_server : public service::typical_server {
        std::map<std::uint64_t, epoc::socket::host> hosts_;

    public:
        explicit socket_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;

        epoc::socket::host &host_by_info(const std::uint32_t family, const std::uint32_t protocol);
    };

    using socket_subsession_instance = std::unique_ptr<epoc::socket::socket_subsession>;

    struct socket_client_session : public service::typical_session {
    private:
        friend class epoc::socket::socket_host_resolver;
        common::identity_container<socket_subsession_instance> subsessions_;

    public:
        explicit socket_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);
        bool is_oldarch();

        void fetch(service::ipc_context *ctx) override;

        void hr_create(service::ipc_context *ctx, const bool with_conn);
        void pr_find(service::ipc_context *ctx);
        void sr_get_by_number(eka2l1::service::ipc_context *ctx);
        void cn_get_long_des_setting(eka2l1::service::ipc_context *ctx);
    };
}
