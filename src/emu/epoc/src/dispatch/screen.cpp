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

#include <epoc/dispatch/screen.h>
#include <common/log.h>

#include <epoc/services/window/window.h>
#include <epoc/kernel.h>
#include <epoc/epoc.h>
#include <fstream>

namespace eka2l1::dispatch {
    BRIDGE_FUNC_DISPATCHER(void, update_screen, const std::uint32_t screen_number, const std::uint32_t num_rects, const eka2l1::rect *rect_list) {
        auto winserv = reinterpret_cast<eka2l1::window_server*>(sys->get_kernel_system()->get_by_name
            <service::server>("!Windowserver"));

        epoc::screen *scr = winserv->get_screens();

        while (scr != nullptr) {
            if (scr->number == screen_number - 1) {
                std::ofstream test("test.bin", std::ios::binary);
                test.write(reinterpret_cast<const char*>(scr->screen_buffer_chunk->host_base()),
                    scr->size().x * scr->size().y * 4);

                LOG_TRACE("Screen dumped");
            }

            scr = scr->next;
        }
    }
}