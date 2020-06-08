/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
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

#include <chrono>
#include <drivers/graphics/backend/emu_controller_glfw.h>
#include <thread>

namespace eka2l1 {
    namespace drivers {
        emu_controller_glfw3::emu_controller_glfw3() {}

        void emu_controller_glfw3::poll() {
            for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; jid++) {
                if (glfwJoystickPresent(jid)) {
                    int axis_count, button_count;
                    const unsigned char *buttons;
                    const float *axes;
                    axes = glfwGetJoystickAxes(jid, &axis_count);
                    buttons = glfwGetJoystickButtons(jid, &button_count);
                    if (gamepads.count(jid) == 0) {
                        gamepads[jid] = { std::vector<bool>(button_count, false),
                            std::vector<float>(axis_count, 0.0f) };
                    } else {
                        if (gamepads[jid].button.size() != button_count) {
                            gamepads[jid].button = std::vector<bool>(button_count, false);
                        }
                        if (gamepads[jid].axis.size() != button_count) {
                            gamepads[jid].axis = std::vector<float>(axis_count, 0.0f);
                        }
                    }
                    gamepad_state &current_pad = gamepads[jid];
                    for (int i = 0; i < button_count; i++) {
                        if (buttons[i] == GLFW_PRESS != current_pad.button[i]) {
                            current_pad.button[i] = buttons[i] == GLFW_PRESS ? true : false;
                            on_button_event(jid, i, buttons[i]);
                        }
                    }
                    for (int i = 0; i < axis_count; i++) {
                        if (abs(axes[i] - current_pad.axis[i]) > 0.05) {
                            if (current_pad.axis[i] < 0.5 && axes[i] >= 0.5) {
                                on_button_event(jid, button_count + i * 2, true);
                            } else if (current_pad.axis[i] >= 0.5 && axes[i] < 0.5) {
                                on_button_event(jid, button_count + i * 2, false);
                            } else if (current_pad.axis[i] > -0.5 && axes[i] <= -0.5) {
                                on_button_event(jid, button_count + i * 2 + 1, true);
                            } else if (current_pad.axis[i] <= -0.5 && axes[i] > -0.5) {
                                on_button_event(jid, button_count + i * 2 + 1, false);
                            }
                            current_pad.axis[i] = axes[i];
                        }
                    }
                } else if (gamepads.count(jid) > 0) {
                    gamepads.erase(jid);
                }
            }
        }

        void emu_controller_glfw3::run() {
            while (true && !shall_stop) {
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
                poll();
            }
        }

        void emu_controller_glfw3::start_polling() {
            shall_stop = false;
            std::thread t(&emu_controller_glfw3::run, this);
            t.detach();
        }

        void emu_controller_glfw3::stop_polling() {
            shall_stop = true;
        }
    }
}
