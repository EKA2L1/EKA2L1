/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <epoc/services/hwrm/hwrm.h>

namespace eka2l1 {    
    hwrm_session::hwrm_session(service::typical_server *serv, service::uid client_ss_uid) 
        : service::typical_session(serv, client_ss_uid) {
    }

    void hwrm_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        default: {
            LOG_ERROR("Unimplemented opcode for HWMR session 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }    

    hwrm_server::hwrm_server(system *sys)
        : service::typical_server(sys, "!HWRMServer") {
        // konna koto ii na
    }

    void hwrm_server::connect(service::ipc_context &ctx) {
        create_session<hwrm_session>(&ctx);
        typical_server::connect(ctx);
    }
}