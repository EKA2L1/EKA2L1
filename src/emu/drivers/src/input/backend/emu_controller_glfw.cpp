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

#include <algorithm>
#include <chrono>
#include <cmath>

#include <drivers/input/backend/emu_controller_glfw.h>

namespace eka2l1 {
    namespace drivers {
        emu_controller_glfw3::emu_controller_glfw3()
            : ANALOG_MIN_DIFFERENCE(0.05f)
            , ANALOG_ACKNOWLEDGE_THRESHOLD(0.5f) {}

        void emu_controller_glfw3::poll() {
            for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; jid++) {
                if (glfwJoystickPresent(jid)) {
                    int axis_count = 0, button_count = 0;
                    const float *axes = glfwGetJoystickAxes(jid, &axis_count);
                    const unsigned char *buttons = glfwGetJoystickButtons(jid, &button_count);

                    // Only allow the maximum of 6 axes, 2 being LT and RT, treats them as button
                    axis_count = std::min<int>(axis_count, 6);

                    if (gamepads.count(jid) == 0) {
                        gamepads[jid] = { std::vector<bool>(button_count, false),
                            std::vector<float>(axis_count, 0.0f) };
                    } else {
                        if (gamepads[jid].button.size() != button_count) {
                            gamepads[jid].button = std::vector<bool>(button_count, false);
                        }
                        if (gamepads[jid].axis.size() != axis_count) {
                            gamepads[jid].axis = std::vector<float>(axis_count, 0.0f);
                        }
                    }
                    gamepad_state &current_pad = gamepads[jid];
                    for (int i = 0; i < button_count; i++) {
                        if ((buttons[i] == GLFW_PRESS) != current_pad.button[i]) {
                            current_pad.button[i] = (buttons[i] == GLFW_PRESS) ? true : false;
                            on_button_event(jid, i, buttons[i]);
                        }
                    }
                    for (int i = 0; i < axis_count; i++) {
                        if (std::fabs(axes[i] - current_pad.axis[i]) > ANALOG_MIN_DIFFERENCE) {
                            if (i < GLFW_GAMEPAD_AXIS_LEFT_TRIGGER) {
                                if (current_pad.axis[i] < ANALOG_ACKNOWLEDGE_THRESHOLD && axes[i] >= ANALOG_ACKNOWLEDGE_THRESHOLD) {
                                    on_button_event(jid, CONTROLLER_BUTTON_CODE_JOYSTICK_BASE + i * 2, true);
                                } else if (current_pad.axis[i] >= ANALOG_ACKNOWLEDGE_THRESHOLD && axes[i] < ANALOG_ACKNOWLEDGE_THRESHOLD) {
                                    on_button_event(jid, CONTROLLER_BUTTON_CODE_JOYSTICK_BASE + i * 2, false);
                                } else if (current_pad.axis[i] > -ANALOG_ACKNOWLEDGE_THRESHOLD && axes[i] <= -ANALOG_ACKNOWLEDGE_THRESHOLD) {
                                    on_button_event(jid, CONTROLLER_BUTTON_CODE_JOYSTICK_BASE + i * 2 + 1, true);
                                } else if (current_pad.axis[i] <= -ANALOG_ACKNOWLEDGE_THRESHOLD && axes[i] > -ANALOG_ACKNOWLEDGE_THRESHOLD) {
                                    on_button_event(jid, CONTROLLER_BUTTON_CODE_JOYSTICK_BASE + i * 2 + 1, false);
                                }
                            } else {
                                on_button_event(jid, CONTROLLER_BUTTON_CODE_LEFT_TRIGGER + (i - GLFW_GAMEPAD_AXIS_LEFT_TRIGGER), (axes[i] > current_pad.axis[i]));
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
            while (!shall_stop) {
                std::this_thread::sleep_for(std::chrono::milliseconds(15));
                poll();
            }
        }

        void emu_controller_glfw3::start_polling() {
            shall_stop = false;
            polling_thread = std::make_unique<std::thread>(&emu_controller_glfw3::run, this);
        }

        void emu_controller_glfw3::stop_polling() {
            shall_stop = true;
            polling_thread->join();
        }

        const char *emu_controller_glfw3::button_to_string(const int button_code) {
            const char *FAST_LOOKUP_SIMPLE[] = {
                "A",
                "B",
                "X",
                "Y",
                "LB",
                "RB",
                "Back",
                "Start",
                "Guide",
                "LT",
                "RT",
                "Dpad Up",
                "Dpad Right",
                "Dpad Down",
                "Dpad Left"
            };

            if (button_code <= GLFW_GAMEPAD_BUTTON_LAST) {
                return FAST_LOOKUP_SIMPLE[button_code];
            }

            switch (button_code) {
            case CONTROLLER_BUTTON_CODE_LEFT_STICK_DOWN:
                return "Left joystick Down";

            case CONTROLLER_BUTTON_CODE_LEFT_STICK_LEFT:
                return "Left joystick Left";

            case CONTROLLER_BUTTON_CODE_LEFT_STICK_RIGHT:
                return "Left joystick Right";

            case CONTROLLER_BUTTON_CODE_LEFT_STICK_UP:
                return "Left joystick Up";

            case CONTROLLER_BUTTON_CODE_RIGHT_STICK_DOWN:
                return "Right joystick Down";

            case CONTROLLER_BUTTON_CODE_RIGHT_STICK_LEFT:
                return "Right joystick Left";

            case CONTROLLER_BUTTON_CODE_RIGHT_STICK_RIGHT:
                return "Right joystick Right";

            case CONTROLLER_BUTTON_CODE_RIGHT_STICK_UP:
                return "Right joystick Up";

            case CONTROLLER_BUTTON_CODE_LEFT_TRIGGER:
                return "Left trigger";

            case CONTROLLER_BUTTON_CODE_RIGHT_TRIGGER:
                return "Right trigger";

            default:
                break;
            }

            return nullptr;
        }
    }
}
