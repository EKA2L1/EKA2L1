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

#include <services/window/classes/plugins/graphics.h>
#include <services/window/window.h>
#include <services/window/op.h>
#include <services/window/opheader.h>
#include <utils/err.h>

namespace eka2l1::epoc {
    bool graphics_piece::execute_command(service::ipc_context &ctx, ws_cmd &cmd) {
        ws_graphic_drawer_opcode op = static_cast<ws_graphic_drawer_opcode>(cmd.header.op);
        bool quit = false;

        switch (op) {
        case ws_graphic_drawer_free: {
            ctx.complete(epoc::error_none);
            client->delete_object(cmd.obj_handle);

            quit = true;
            break;
        }

        case ws_graphic_drawer_get_graphic_id: {
            LOG_TRACE(SERVICE_WINDOW, "Graphics drawer get graphic id stubbed");

            ws_cmd_graphic_drawer_graphic_id id_var;
            id_var.id = 1;
            id_var.is_uid = 0;

            ctx.write_data_to_descriptor_argument(reply_slot, id_var, nullptr, true);
            ctx.complete(epoc::error_none);

            break;
        }

        default: {
            // The number of unimplemented is too small, better just complete all
            LOG_TRACE(SERVICE_WINDOW, "Unimplemented graphic drawer opcode: 0x{:x}", cmd.header.op);
            ctx.complete(epoc::error_none);

            break;
        }
        }

        return quit;
    }

    graphics_piece::graphics_piece(window_server_client_ptr client, screen *scr)
        : window_client_obj(client, scr) {
    }
}