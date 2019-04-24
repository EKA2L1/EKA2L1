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

#include <epoc/services/window/classes/plugins/animdll.h>
#include <epoc/services/window/op.h>

#include <common/e32inc.h>
#include <e32err.h>

namespace eka2l1::epoc {
    void anim_dll::execute_command(service::ipc_context &ctx, ws_cmd cmd) {
        TWsAnimDllOpcode op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsAnimDllOpCreateInstance: {
            LOG_TRACE("AnimDll::CreateInstance stubbed with a anim handle (>= 0)");
            ctx.set_request_status(user_count++);

            break;
        }

        case EWsAnimDllOpCommandReply: {
            LOG_TRACE("AnimDll command reply stubbed!");
            ctx.set_request_status(KErrNone);

            break;
        }

        default: {
            LOG_ERROR("Unimplement AnimDll Opcode: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }
}
