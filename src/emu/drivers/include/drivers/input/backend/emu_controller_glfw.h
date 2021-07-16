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

#include <drivers/input/emu_controller.h>

#include <map>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <GLFW/glfw3.h>

namespace eka2l1 {
    namespace drivers {
        enum controller_button_code {
            CONTROLLER_BUTTON_CODE_JOYSTICK_BASE = 300,
            CONTROLLER_BUTTON_CODE_LEFT_STICK_RIGHT = CONTROLLER_BUTTON_CODE_JOYSTICK_BASE,
            CONTROLLER_BUTTON_CODE_LEFT_STICK_LEFT,
            CONTROLLER_BUTTON_CODE_LEFT_STICK_DOWN,
            CONTROLLER_BUTTON_CODE_LEFT_STICK_UP,
            CONTROLLER_BUTTON_CODE_RIGHT_STICK_RIGHT,
            CONTROLLER_BUTTON_CODE_RIGHT_STICK_LEFT,
            CONTROLLER_BUTTON_CODE_RIGHT_STICK_DOWN,
            CONTROLLER_BUTTON_CODE_RIGHT_STICK_UP,
            CONTROLLER_BUTTON_CODE_LEFT_TRIGGER,
            CONTROLLER_BUTTON_CODE_RIGHT_TRIGGER
        };

        class emu_controller_glfw3 : public emu_controller {
            struct gamepad_state {
                // true if pressed
                std::vector<bool> button;
                std::vector<float> axis;
            };

            std::map<int, gamepad_state> gamepads;
            std::atomic<bool> shall_stop;
            std::unique_ptr<std::thread> polling_thread;
            const float ANALOG_MIN_DIFFERENCE;
            const float ANALOG_ACKNOWLEDGE_THRESHOLD;

            void poll();
            void run();

        public:
            explicit emu_controller_glfw3();
            void start_polling() override;
            void stop_polling() override;
            const char *button_to_string(const int button_code) override;
        };
    }
}
