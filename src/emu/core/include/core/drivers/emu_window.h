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
#pragma once

#include <common/vecx.h>
#include <cstdint>

namespace eka2l1 {
    namespace driver {
        class emu_window {
            // EPOC9 and down
            virtual void init() = 0;

            virtual void set_framebuffer_pixel(const point &pix, uint8_t color) = 0;
            virtual void clear(const uint8_t color) = 0;

            virtual void render() = 0;
            virtual void swap_buffer() = 0;

            virtual void shutdown() = 0;
        };
    }
}
