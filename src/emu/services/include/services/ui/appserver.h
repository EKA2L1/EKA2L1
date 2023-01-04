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

#include <services/framework.h>
#include <utils/reqsts.h>

namespace eka2l1 {
    enum app_ui_based_opcode {
        app_ui_based_notify_app_exit = 1,
        app_ui_based_cancel_notify_app_exit
    };

    class app_ui_based_session: public service::typical_session {
    private:
        epoc::notify_info notify_exit_;
        int exit_reason_;

    public:
        explicit app_ui_based_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_version);
        virtual ~app_ui_based_session();
        
        void fetch(service::ipc_context *ctx) override;

        void complete_server_exit_notify(int error_code);
    };

    class app_ui_based_server: public service::typical_server {
    public:
        explicit app_ui_based_server(system *sys, std::uint32_t server_differentiator, std::uint32_t app_uid);
        virtual ~app_ui_based_server();

        void notify_app_exit(int error_code);
    };
}