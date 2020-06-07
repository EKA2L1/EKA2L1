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

#include <services/window/classes/wsobj.h>
#include <services/window/common.h>

#include <memory>

namespace eka2l1::epoc {
    struct screen;
    struct window_group;

    /**
     * \brief Represents a screen.
     */
    struct screen_device : public window_client_obj {
        void execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd) override;

        void set_screen_mode_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void set_screen_mode(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void get_screen_size_mode_list(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void get_screen_size_mode_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd,
            const bool bonus_the_twips);
        void get_default_screen_size_and_rotation(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd,
            const bool twips);
        void get_current_screen_mode_scale(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void get_default_screen_mode_origin(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);
        void is_screen_mode_dynamic(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd &cmd);

        explicit screen_device(window_server_client_ptr client, epoc::screen *scr);
    };
}
