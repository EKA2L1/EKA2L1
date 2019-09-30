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

#include <epoc/epoc.h>
#include <epoc/services/hwrm/hwrm.h>
#include <epoc/services/hwrm/op.h>
#include <epoc/services/hwrm/light/light.h>

#include <common/e32inc.h>
#include <e32err.h>

namespace eka2l1 {    
    hwrm_session::hwrm_session(service::typical_server *serv, service::uid client_ss_uid) 
        : service::typical_session(serv, client_ss_uid) {
    }

    void hwrm_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case hwrm_fundamental_op_create_light_service: {
            resource_ = std::make_unique<epoc::light_resource>(ctx->sys->get_kernel_system());
            ctx->set_request_status(KErrNone);
            break;
        }

        default: {
            int cmd_num = ctx->msg->function;

            if (cmd_num >= 1000) {
                // TODO: Check back with later version. For S60v5 light command base is 2000
                if (cmd_num >= 2000 && cmd_num <= 3000) {
                    cmd_num -= 1000;
                }

                ctx->msg->function = cmd_num;
                resource_->execute_command(*ctx);
            } else {
                LOG_ERROR("Unimplemented opcode for HWMR session 0x{:X}", ctx->msg->function);
            }

            break;
        }
        }
    }    

    hwrm_server::hwrm_server(system *sys)
        : service::typical_server(sys, "!HWRMServer") {
        // konna koto ii na
    }

    void hwrm_server::connect(service::ipc_context &ctx) {
        if (!light_data_) {
            if (!init()) {
                LOG_ERROR("Fail to initialise HWRM service shared data!");
            }
        }

        create_session<hwrm_session>(&ctx);
        typical_server::connect(ctx);
    }

    bool hwrm_server::init() {
        // Initialise light common data
        light_data_ = std::make_unique<epoc::hwrm::light::resource_data>(sys->get_kernel_system());
        return true;
    }
}