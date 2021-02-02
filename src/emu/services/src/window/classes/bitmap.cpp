/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/fbs/fbs.h>
#include <services/window/classes/bitmap.h>
#include <services/window/window.h>

#include <utils/err.h>

namespace eka2l1::epoc {
    wsbitmap::wsbitmap(window_server_client_ptr client, fbsbitmap *bmp)
        : window_client_obj(client, nullptr)
        , bitmap_(bmp) {
        bmp->ref();
    }

    wsbitmap::~wsbitmap() {
        bitmap_->deref();
    }

    bool wsbitmap::execute_command(service::ipc_context &context, ws_cmd &cmd) {
        bool quit = false;

        if (cmd.header.op == 0) {
            // Destroy
            context.complete(epoc::error_none);
            client->delete_object(cmd.obj_handle);
            
            quit = true;
        } else {
            LOG_ERROR(SERVICE_WINDOW, "Unimplemented wsbitmap opcode {}", cmd.header.op);
        }

        return quit;
    }
}