/*
 * Copyright (c) 2019 EKA2L1 Team
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
#include <services/window/common.h>
#include <string>
#include <vector>

namespace eka2l1::epoc {
    namespace config {
        struct screen_mode {
            int screen_number;
            int mode_number;

            /// The size of the screen, assigned by native resolution config
            eka2l1::vec2 size;

            /// The size of the screen, unmodified
            eka2l1::vec2 size_original;

            int rotation;

            std::string style;
        };

        struct hardware_state {
            int state_number;
            int mode_normal;
            int mode_alternative;
            int switch_keycode;
        };

        struct screen {
            int screen_number;
            epoc::display_mode disp_mode;

            bool auto_clear;
            bool flicker_free;
            bool blt_offscreen;

            std::vector<screen_mode> modes;
            std::vector<hardware_state> hardware_states;
        };
    }
}
