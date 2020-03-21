/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <epoc/dispatch/dispatcher.h>
#include <epoc/dispatch/screen.h>
#include <common/log.h>

#include <drivers/graphics/graphics.h>
#include <epoc/services/window/common.h>
#include <epoc/services/window/window.h>
#include <epoc/kernel.h>
#include <epoc/epoc.h>
#include <fstream>

namespace eka2l1::dispatch {
    BRIDGE_FUNC_DISPATCHER(void, update_screen, const std::uint32_t screen_number, const std::uint32_t num_rects, const eka2l1::rect *rect_list) {
        dispatch::dispatcher *dispatcher = sys->get_dispatcher();
        drivers::graphics_driver *driver = sys->get_graphics_driver();
        epoc::screen *scr = dispatcher->winserv_->get_screens();

        while (scr != nullptr) {
            if (scr->number == screen_number - 1) {
                // Update the DSA screen texture
                const eka2l1::vec2 screen_size = scr->size();
                const std::size_t buffer_size = scr->size().x * scr->size().y * 4;

                if (!scr->dsa_texture) {
                    scr->dsa_texture = drivers::create_bitmap(driver, screen_size);
                }

                driver->update_bitmap(scr->dsa_texture, buffer_size, 0, screen_size,
                    epoc::get_bpp_from_display_mode(scr->disp_mode), scr->screen_buffer_chunk->host_base());
            }

            scr = scr->next;
        }
    }
}