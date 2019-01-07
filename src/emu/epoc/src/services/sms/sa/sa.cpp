/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <epoc/services/sms/sa/sa.h>

#include <common/e32inc.h>
#include <e32err.h>

namespace eka2l1 {
    void sa_server::unk_op1(service::ipc_context ctx) {
        // If it's not working out, we should use this
        //
        // arm_xemitter emitter;
        // hle::lib_manager *libmngr = ctx.sys->get_lib_manager();
        // address append_func = libmngr->get_func_addr("ID_OF_EUSER_RARRAYBASE_APPEND");
        //
        // alloc global memory feature
        // emitter.PUSH() -- Preserve all registers
        //
        // emitter.MOV(R0, memory);
        //
        // fill in feature
        // make a loop
        //
        // emitter.BL(append_func)
        //
        // emitter.POP() -- delete all

        ctx.set_request_status(KErrNone);
    }

    sa_server::sa_server(eka2l1::system *sys)
        : service::server(sys, "SAServer", true) {
        REGISTER_IPC(sa_server, unk_op1, 1001, "SaServer::UnkOp1");
    }
}