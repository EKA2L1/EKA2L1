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

#include <epoc/services/window/common.h>
#include <epoc/services/window/classes/wsobj.h>
#include <epoc/services/window/window.h>        ///< Unwanted include
#include <common/log.h>

namespace eka2l1::epoc {
    window_client_obj::window_client_obj(window_server_client_ptr client)
        : client(client)
        , id(static_cast<std::uint32_t>(client->objects.size()) + base_handle + 1) {
    }

    void window_client_obj::execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd cmd) {
        LOG_ERROR("Unimplemented command handler for object with handle: 0x{:x}", cmd.obj_handle);
    }
}