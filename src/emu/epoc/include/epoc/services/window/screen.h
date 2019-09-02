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

#include <common/vecx.h>
#include <drivers/graphics/common.h>
#include <epoc/services/window/common.h>
#include <epoc/services/window/classes/config.h>

namespace eka2l1::drivers {
    class graphics_driver;
}

namespace eka2l1::epoc {
    struct window;
    struct window_group;

    struct screen {
        int number;

        // The root window, used to traverse window tree
        // Draw order will be child in front of parent, newer in front of older.
        epoc::window *root;
        eka2l1::vec2 size;
        drivers::handle screen_texture; ///< Server handle to texture of the screen
        epoc::display_mode disp_mode;

        epoc::config::screen scr_config;        ///< All mode of this screen
        epoc::config::screen_mode *crr_mode;    ///< The current mode being used by the screen.

        epoc::window_group *focus;              ///< Current window group that is being focused

        epoc::display_mode disp_mode{ display_mode::color16ma };

        explicit screen(const eka2l1::vec2 &size);

        /**
         * \brief Redraw the screen.
         * \param driver Pointer to the graphics driver
         */
        void redraw(drivers::graphics_driver *driver);
    };
}