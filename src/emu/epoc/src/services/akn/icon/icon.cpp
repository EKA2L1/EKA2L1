/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <epoc/services/akn/icon/icon.h>
#include <epoc/services/akn/icon/ops.h>

#include <common/e32inc.h>
#include <e32err.h>

namespace eka2l1 {
    akn_icon_server_session::akn_icon_server_session(service::typical_server *svr, service::uid client_ss_uid) 
        : service::typical_session(svr, client_ss_uid) {
    }

    void akn_icon_server_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case akn_icon_server_get_init_data: {
            // Write initialisation data to buffer
            ctx->write_arg_pkg<epoc::akn_icon_init_data>(0, *server<akn_icon_server>()->get_init_data()
                , nullptr, true);
            ctx->set_request_status(KErrNone);
            
            break;
        }

        default: {
            LOG_ERROR("Unimplemented IPC opcode for AknIconServer session: 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    akn_icon_server::akn_icon_server(eka2l1::system *sys)
        : service::typical_server(sys, "!AknIconServer") {
    }
    
    void akn_icon_server::connect(service::ipc_context context) {
        if (!flags & akn_icon_srv_flag_inited) {
            init_server();
        }
        
        create_session<akn_icon_server_session>(&context);
        context.set_request_status(KErrNone);
    }
}
