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

#pragma once

#include <services/window/classes/wsobj.h>

namespace eka2l1 {
    struct fbsbitmap;
    class fbs_server;

    namespace epoc {
        struct wsbitmap : public window_client_obj {
            fbsbitmap *bitmap_;

            explicit wsbitmap(window_server_client_ptr client, fbsbitmap *bmp);
            ~wsbitmap() override;

            bool execute_command(service::ipc_context &context, ws_cmd &cmd) override;
        };
    }
}