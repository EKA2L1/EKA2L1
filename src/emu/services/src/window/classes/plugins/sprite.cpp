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

#include <services/window/classes/plugins/sprite.h>

namespace eka2l1::epoc {
    bool sprite::execute_command(service::ipc_context &context, ws_cmd &cmd) {
        bool quit = false;
        return quit;
    }

    sprite::sprite(window_server_client_ptr client, screen *scr, window *attached_window, eka2l1::vec2 pos)
        : window_client_obj(client, scr)
        , position(pos)
        , attached_window(attached_window) {
    }
}