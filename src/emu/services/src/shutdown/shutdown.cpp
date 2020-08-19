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

#include <services/shutdown/shutdown.h>

#include <epoc/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    std::string get_shutdown_server_name_through_epocver(const epocver ver) {
        if (ver < epocver::eka2) {
            return "ShutdownServer";
        }

        return "!ShutdownServer";
    }

    shutdown_session::shutdown_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver)
        : service::typical_session(svr, client_ss_uid, client_ver) {
        
    }
    
    void shutdown_session::fetch(service::ipc_context *context) {
        LOG_ERROR("Unimplemented shutdown server opcode: {}", context->msg->function);
    }

    shutdown_server::shutdown_server(eka2l1::system *sys)
        : service::typical_server(sys, get_shutdown_server_name_through_epocver(sys->get_symbian_version_use())) {
    }

    void shutdown_server::connect(service::ipc_context &context) {
        create_session<shutdown_session>(&context);
        context.complete(epoc::error_none);
    }
}