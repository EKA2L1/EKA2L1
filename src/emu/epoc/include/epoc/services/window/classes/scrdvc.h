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

#include <epoc/services/window/classes/config.h>
#include <epoc/services/window/classes/wsobj.h>
#include <epoc/services/window/common.h>

#include <drivers/graphics/graphics.h>
#include <drivers/itc.h>

#include <memory>

namespace eka2l1::epoc {
    struct window_group;
    using window_group_ptr = std::shared_ptr<epoc::window_group>;

    struct window;
    using window_ptr = std::shared_ptr<epoc::window>;

    struct screen_device : public window_client_obj {
        eka2l1::graphics_driver_client_ptr driver;
        int screen;

        epoc::config::screen scr_config;
        epoc::config::screen_mode *crr_mode;

        std::vector<epoc::window_ptr> windows;
        epoc::window_group_ptr focus;

        epoc::window_group_ptr find_window_group_to_focus();

        void update_focus(epoc::window_group_ptr closing_group);

        screen_device(window_server_client_ptr client, int number,
            eka2l1::graphics_driver_client_ptr driver);

        void execute_command(eka2l1::service::ipc_context &ctx, eka2l1::ws_cmd cmd) override;
    };
}