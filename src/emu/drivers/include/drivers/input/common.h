/*
 * Copyright (c) 2019 EKA2L1 Team.
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

namespace eka2l1::drivers {
    enum class input_event_type {
        none,
        key,
        key_raw,
        touch,
        button
    };

    enum class key_state {
        pressed,
        released,
        repeat
    };

    enum class button_state {
        pressed,
        released
    };

    enum mouse_button {
        mouse_button_1 = 0,
        mouse_button_2 = 1,
        mouse_button_3 = 2,
        mouse_button_4 = 3,
        mouse_button_5 = 4,
        mouse_button_6 = 5,
        mouse_button_7 = 6,
        mouse_button_8 = 7,
        mouse_button_9 = 8,
        mouse_button_10 = 9,
        mouse_button_left = mouse_button_1,
        mouse_button_right = mouse_button_2,
        mouse_button_middle = mouse_button_3
    };

    enum mouse_action {
        mouse_action_press,
        mouse_action_repeat,
        mouse_action_release
    };

    enum controller_button_code {
        CONTROLLER_BUTTON_CODE_A = 0,
        CONTROLLER_BUTTON_CODE_B = 1,
        CONTROLLER_BUTTON_CODE_X = 2,
        CONTROLLER_BUTTON_CODE_Y = 3,
        CONTROLLER_BUTTON_CODE_LB = 4,
        CONTROLLER_BUTTON_CODE_RB = 5,
        CONTROLLER_BUTTON_CODE_BACK = 6,
        CONTROLLER_BUTTON_CODE_START = 7,
        CONTROLLER_BUTTON_CODE_GUIDE = 8,
        CONTROLLER_BUTTON_CODE_LT = 9,
        CONTROLLER_BUTTON_CODE_RT = 10,
        CONTROLLER_BUTTON_CODE_DPAD_UP = 11,
        CONTROLLER_BUTTON_CODE_DPAD_RIGHT = 12,
        CONTROLLER_BUTTON_CODE_DPAD_DOWN = 13,
        CONTROLLER_BUTTON_CODE_DPAD_LEFT = 14,
        CONTROLLER_BUTTON_CODE_MISC1 = 15,
        CONTROLLER_BUTTON_CODE_PADDLE1 = 16,
        CONTROLLER_BUTTON_CODE_PADDLE2 = 17,
        CONTROLLER_BUTTON_CODE_PADDLE3 = 18,
        CONTROLLER_BUTTON_CODE_PADDLE4 = 19,
        CONTROLLER_BUTTON_CODE_TOUCHPAD = 20,
        CONTROLLER_BUTTON_CODE_STANDARD_LAST,
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

    /**
     * Event for key press/key release.
     */
    struct key_event {
        int code_;
        key_state state_;
    };

    /**
     * Event for controller button press/release.
     */
    struct button_event {
        int controller_;
        int button_;
        button_state state_;
    };

    /**
     * Event for mouse 0/1/2 button press/move/release.
     */
    struct mouse_event {
        // For pos z. Negative is proximity. Positive is pressure
        int pos_x_, pos_y_, pos_z_;
        mouse_button button_;
        mouse_action action_;
        std::uint32_t mouse_id;
    };

    struct input_event {
        input_event_type type_;
        std::uint64_t time_;

        union {
            key_event key_;
            mouse_event mouse_;
            button_event button_;
        };
    };

    const char *button_to_string(const controller_button_code button_code);
}