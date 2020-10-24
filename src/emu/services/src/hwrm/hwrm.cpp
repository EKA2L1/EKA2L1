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

#include <system/epoc.h>
#include <services/hwrm/hwrm.h>
#include <services/hwrm/light/light.h>
#include <services/hwrm/op.h>
#include <services/hwrm/vibration/vibration.h>
#include <utils/err.h>



namespace eka2l1 {
    hwrm_session::hwrm_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(serv, client_ss_uid, client_version) {
    }

    void hwrm_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case hwrm_fundamental_op_create_vibration_service: {
            resource_ = std::make_unique<epoc::vibration_resource>(ctx->sys->get_kernel_system());
            ctx->complete(epoc::error_none);
            break;
        }

        case hwrm_fundamental_op_create_light_service: {
            resource_ = std::make_unique<epoc::light_resource>(ctx->sys->get_kernel_system());
            ctx->complete(epoc::error_none);
            break;
        }

        default: {
            if (ctx->msg->function >= 1000) {
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
        if (!light_data_ || !vibration_data_) {
            if (!init()) {
                LOG_ERROR("Fail to initialise HWRM service shared data!");
            }
        }

        create_session<hwrm_session>(&ctx);
        typical_server::connect(ctx);
    }

    bool hwrm_server::init() {
        // Initialise light common data
        kernel_system *kern = sys->get_kernel_system();

        light_data_ = std::make_unique<epoc::hwrm::light::resource_data>(kern);
        vibration_data_ = std::make_unique<epoc::hwrm::vibration::resource_data>(kern, sys->get_io_system(),
            sys->get_device_manager());

        return true;
    }
}