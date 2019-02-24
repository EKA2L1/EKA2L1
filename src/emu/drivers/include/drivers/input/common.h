/*
 * Copyright (c) 2019 EKA2L1 Team.
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

namespace eka2l1::drivers {
    enum class input_event_type {
        key,
        touch
    };

    enum key_state {
        pressed,
        released,
        repeat
    };

    enum key_scancode {
        // TODO: Need more codes for QWERTY and etc...
        key_mid_button = 180,
        key_left_button = 196,
        key_right_button = 197
    };

    /**
     * Event for key press/key release.
     */
    struct key_event {
        key_scancode code_;
        key_state state_;
    };

    struct input_event {
        input_event_type type_;

        union {
            key_event key_;
        };
    };
}