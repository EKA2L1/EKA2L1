/*
 * Copyright (c) 2023 EKA2L1 Team
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

#include <services/ui/appserver.h>
#include <string>

namespace eka2l1 {
    enum browser_for_app_opcode {
        browser_for_app_open_url = 0x201,
        browser_for_app_get_state_after_close = 0x204
    };

    class browser_for_app_session : public app_ui_based_session {
    public:
        explicit browser_for_app_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_version);
        void fetch(service::ipc_context *ctx) override;

        void open_url(service::ipc_context *ctx);
    };

    class browser_for_app_server: public app_ui_based_server {
    public:
        explicit browser_for_app_server(system *sys, std::uint32_t server_differentiator);
        void connect(service::ipc_context &ctx) override;
    };
}