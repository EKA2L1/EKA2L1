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

#include <services/window/classes/plugins/clickdll.h>
#include <services/window/op.h>

#include <common/cvt.h>
#include <common/log.h>

#include <utils/err.h>

namespace eka2l1::epoc {
    bool click_dll::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        TWsClickOpcodes op = static_cast<decltype(op)>(cmd.header.op);
        bool quit = false;

        switch (op) {
        case EWsClickOpIsLoaded: {
            ctx.complete(loaded ? 0 : 0x1);
            break;
        }

        case EWsClickOpLoad: {
            int dll_click_name_length = *reinterpret_cast<int *>(cmd.data_ptr);
            char16_t *dll_click_name_ptr = reinterpret_cast<char16_t *>(
                reinterpret_cast<std::uint8_t *>(cmd.data_ptr) + 4);

            std::u16string dll_click_name(dll_click_name_ptr, dll_click_name_length);
            LOG_TRACE(SERVICE_WINDOW, "Stubbed EWsClickOpLoad (loading click DLL {})", common::ucs2_to_utf8(dll_click_name));

            ctx.complete(epoc::error_none);

            break;
        }

        case EWsClickOpCommandReply: {
            LOG_TRACE(SERVICE_WINDOW, "ClickOpCommandReply stubbed with KErrNone");
            ctx.complete(epoc::error_none);

            break;
        }

        default: {
            LOG_ERROR(SERVICE_WINDOW, "Unimplemented ClickDll opcode: 0x{:x}", cmd.header.op);
            break;
        }
        }

        return quit;
    }
}
