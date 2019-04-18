/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <common/time.h>
#include <epoc/services/window/common.h>

namespace eka2l1::epoc {
    // TODO: Use emulated time
    event::event(const std::uint32_t handle, event_code evt_code)
        : handle(handle)
        , type(evt_code)
        , time(common::get_current_time_in_microseconds_since_1ad()) {
    }
    
    TKeyCode map_scancode_to_keycode(TStdScanCode scan_code) {
        if (scan_code < EStdKeyF1) {
            return keymap[scan_code];
        } else if (scan_code >= EStdKeyF1 && scan_code <= EStdKeyApplication27) {
            return keymap[scan_code - 67];
        } else {
            return EKeyNull;
        }
    }
}
