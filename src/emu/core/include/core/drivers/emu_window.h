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
#include <functional>
#include <cstdint>

namespace eka2l1 {
    namespace driver {
        class emu_window {
        public:
            virtual void init(std::string title, vec2 size) = 0;
            virtual void make_current() = 0;
            virtual void done_current() = 0;
            virtual void swap_buffer() = 0;
            virtual void poll_events() = 0;
            virtual void shutdown() = 0;

            std::function<void(vec2)> resize_hook;

            /* Callback handler */

            /* Call when a touch input is triggered */
            std::function<void(point)> touch_pressed;

            /* Call when a touch movement is detected */
            std::function<void(point)> touch_move;

            /* Call when touch is released */
            std::function<void()> touch_released;

            /* Call when a button is pressed. User sets their own call, shutdown and center button */
            std::function<void(uint16_t)> button_pressed;

            /* Call when a button is released */
            std::function<void()> button_released;

            /* Call when a button is held */
            std::function<void(uint16_t)> button_hold;
        };
    }
}
