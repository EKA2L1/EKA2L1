/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <drivers/driver.h>
#include <drivers/itc.h>

#include <drivers/graphics/graphics.h>

namespace eka2l1::drivers {
    bool driver_client::send_opcode(const int opcode, itc_context &ctx) {
        driver->request_queue.push({ opcode, ctx });
        return true;
    }

    void driver_client::sync_with_driver() {
        std::unique_lock<std::mutex> ulock(lock);
        driver->cond.wait(ulock);
    }

    driver_client::driver_client(driver_instance driver) 
        : driver(driver) {

    }

    graphics_driver_client::graphics_driver_client(driver_instance driver)
        : driver_client(driver) {

    }

    /*! \brief Get the screen size in pixels
    */
    vec2 graphics_driver_client::screen_size() {
        // Synchronize call should directly use the client
        std::shared_ptr<graphics_driver> gdriver = 
            std::dynamic_pointer_cast<graphics_driver>(driver);

        return gdriver->get_screen_size();
    }

    /*! \brief Clear the screen with color.
        \params color A RGBA vector 4 color
    */
    void graphics_driver_client::clear(vecx<float, 4> color) {
        itc_context context;
        context.push(color);

        send_opcode(graphics_driver_clear, context);
    }
}