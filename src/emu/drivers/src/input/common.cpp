/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#include <drivers/input/common.h>

namespace eka2l1::drivers {
    const char *button_to_string(const controller_button_code button_code) {
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
            "Dpad Left",
            "Misc 1",
            "P1",
            "P2",
            "P3",
            "P4",
            "Touchpad"
        };

        if (button_code < CONTROLLER_BUTTON_CODE_STANDARD_LAST) {
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