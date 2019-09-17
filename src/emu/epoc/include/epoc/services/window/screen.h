/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <common/vecx.h>
#include <drivers/graphics/common.h>
#include <epoc/services/window/common.h>
#include <epoc/services/window/classes/config.h>

namespace eka2l1::drivers {
    class graphics_driver;
    class graphics_command_list_builder;
}

namespace eka2l1::epoc {
    struct window;
    struct window_group;

    struct screen {
        int number;

        // The root window, used to traverse window tree
        // Draw order will be child in front of parent, newer in front of older.
        std::unique_ptr<epoc::window> root;
        drivers::handle screen_texture; ///< Server handle to texture of the screen
        epoc::display_mode disp_mode;

        epoc::config::screen scr_config;        ///< All mode of this screen
        std::uint8_t crr_mode;    ///< The current mode being used by the screen.

        epoc::window_group *focus;              ///< Current window group that is being focused

        screen *next;

        explicit screen(const int number, epoc::config::screen &scr_conf);

        // ========================= UTILITIES FUNCTIONS ===========================
        /**
         * \brief Get the size of this screen, in pixels.
         */
        eka2l1::vec2 size() const;
        
        /**
         * \brief   Retrieve info of current screen mode.
         * \returns Info of current screen mode
         */
        const epoc::config::screen_mode &current_mode() const;

        /**
         * \brief Get the maximum number of modes this screen support.
         */
        const int total_screen_mode() const;

        const epoc::config::screen_mode *mode_info(const int number) const;

        /**
         * \brief Set screen mode.
         */
        void set_screen_mode(drivers::graphics_driver *drv, const int mode);

        /**
         * \brief Resize the screen.
         * 
         * This resize the screen. Which means all pixel will be lose, and redraw will happens, if
         * GPU renderer is enabled.
         * 
         * DSA (Direct Screen Access) will also be lost too, it will not be saved.
         * Note: For now DSA will be lost, yes. But I'm thinking we could alloc a buffer and let user
         * edit pixel there. Well read can kind of be hard though. At the end, upload DSA texture
         * and draw it over all windows.
         * 
         * Does this apply to rotation though? TODO. But most likely we should...
         * 
         * This function also creates the screen bitmap from server side if there is currently none.
         * 
         * WARNING: Don't call this from outside of this struct. Nguy hiểm
         * 
         * \param driver     The graphics driver associated with the screen.
         * \param new_size   Size of the screen.
         */
        void resize(drivers::graphics_driver *driver, const eka2l1::vec2 &new_size);

        void deinit(drivers::graphics_driver *driver);
        void redraw(drivers::graphics_command_list_builder *builder);

        /**
         * \brief Redraw the screen.
         * \param driver Pointer to the graphics driver
         */
        void redraw(drivers::graphics_driver *driver);

        /**
         * \brief Update the window group focus.
         */
        epoc::window_group *update_focus(epoc::window_group *closing_group);
    };
}