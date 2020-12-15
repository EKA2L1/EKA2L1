/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <services/window/classes/plugins/animdll.h>
#include <services/window/op.h>
#include <utils/err.h>

namespace eka2l1::epoc {
    anim_dll::anim_dll(window_server_client_ptr client, screen *scr)
        : window_client_obj(client, scr)
        , user_count(0) {
    }

    void anim_dll::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_anim_dll_opcode op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case ws_anim_dll_op_create_instance: {
            LOG_TRACE(SERVICE_WINDOW, "AnimDll::CreateInstance stubbed with a anim handle (>= 0)");
            ctx.complete(user_count++);

            break;
        }

        case ws_anim_dll_op_command_reply: {
            LOG_TRACE(SERVICE_WINDOW, "AnimDll command reply stubbed!");
            ctx.complete(epoc::error_none);

            break;
        }

        default: {
            LOG_ERROR(SERVICE_WINDOW, "Unimplemented AnimDll opcode: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }
}
