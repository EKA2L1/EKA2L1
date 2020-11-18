/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/framework.h>
#include <utils/reqsts.h>

namespace eka2l1 {
    std::string get_shutdown_server_name_through_epocver(const epocver ver);

    enum shutdown_server_opcode {
        shutdown_server_notify,
        shutdown_server_notify_cancel,
        shutdown_server_handle_error,
        shutdown_server_power_off,
        shutdown_server_query_power_state
    };

    class shutdown_session: public service::typical_session {
        epoc::notify_info nof_;

    public:
        explicit shutdown_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver);
        
        void fetch(service::ipc_context *context) override;
        
        void request_notify(service::ipc_context *context);
        void cancel_notify(service::ipc_context *context);
    };

    class shutdown_server: public service::typical_server {
    public:
        explicit shutdown_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;
    };
}