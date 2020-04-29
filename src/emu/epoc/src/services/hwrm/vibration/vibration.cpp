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

#include <epoc/services/context.h>
#include <epoc/services/hwrm/op.h>
#include <epoc/services/hwrm/vibration/vibration.h>
#include <epoc/utils/err.h>

namespace eka2l1::epoc {
    vibration_resource::vibration_resource(kernel_system *kern)
        : kern_(kern) {
    }

    void vibration_resource::execute_command(service::ipc_context &ctx) {
        LOG_ERROR("Unimplemented operation for vibration resource: ({})", ctx.msg->function);
        ctx.set_request_status(epoc::error_none);
    }
}