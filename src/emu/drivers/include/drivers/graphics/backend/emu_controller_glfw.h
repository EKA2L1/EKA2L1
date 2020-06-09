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

#include <drivers/graphics/emu_controller.h>

#include <map>
#include <vector>
#include <thread>
#include <atomic>
#include <GLFW/glfw3.h>

namespace eka2l1 {
    namespace drivers {
        class emu_controller_glfw3 : public emu_controller {
            struct gamepad_state {
                // true if pressed
                std::vector<bool> button;
                std::vector<float> axis;
            };

            std::map<int, gamepad_state> gamepads;
            std::atomic<bool> shall_stop;
            std::unique_ptr<std::thread> polling_thread;

            void poll();
            void run();

        public:
            explicit emu_controller_glfw3();
            void start_polling() override;
            void stop_polling() override;
        };
    }
}
