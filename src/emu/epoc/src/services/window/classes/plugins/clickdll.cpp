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

#include <epoc/services/window/classes/plugins/clickdll.h>
#include <epoc/services/window/op.h>

#include <common/cvt.h>
#include <common/e32inc.h>
#include <common/log.h>

#include <e32err.h>

namespace eka2l1::epoc {
    void click_dll::execute_command(service::ipc_context &ctx, ws_cmd cmd) {
        TWsClickOpcodes op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsClickOpIsLoaded: {
            ctx.set_request_status(loaded ? 0 : 0x1);
            break;
        }

        case EWsClickOpLoad: {
            int dll_click_name_length = *reinterpret_cast<int *>(cmd.data_ptr);
            char16_t *dll_click_name_ptr = reinterpret_cast<char16_t *>(
                reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + 4);

            std::u16string dll_click_name(dll_click_name_ptr, dll_click_name_length);
            LOG_TRACE("Stubbed EWsClickOpLoad (loading click DLL {})", common::ucs2_to_utf8(dll_click_name));

            ctx.set_request_status(KErrNone);

            break;
        }

        case EWsClickOpCommandReply: {
            LOG_TRACE("ClickOpCommandReply stubbed with KErrNone");
            ctx.set_request_status(KErrNone);

            break;
        }

        default: {
            LOG_ERROR("Unimplement ClickDll Opcode: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }
}
