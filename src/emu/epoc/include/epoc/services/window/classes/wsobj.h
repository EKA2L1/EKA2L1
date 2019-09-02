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

#pragma once

#include <epoc/services/context.h>
#include <epoc/services/window/opheader.h>

#include <cstdint>

namespace eka2l1::epoc {
    namespace ws {
        using uid = std::uint32_t;
        using handle = std::uint32_t;
    };

    class window_server_client;
    using window_server_client_ptr = window_server_client *;

    struct screen;

    struct window_client_obj {
        ws::uid id;
        ws::handle owner_handle;

        window_server_client *client;
        screen *scr;

        explicit window_client_obj(window_server_client_ptr client, screen *scr);
        virtual ~window_client_obj() {}

        virtual void execute_command(eka2l1::service::ipc_context &ctx,
            eka2l1::ws_cmd cmd);
    };
}
