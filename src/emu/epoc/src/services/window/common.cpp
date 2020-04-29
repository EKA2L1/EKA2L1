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

    bool is_display_mode_color(const epoc::display_mode disp_mode) {
        return disp_mode >= epoc::display_mode::color16;
    }

    bool is_display_mode_mono(const epoc::display_mode disp_mode) {
        return disp_mode <= epoc::display_mode::gray256;
    }
    
    bool is_display_mode_alpha(const display_mode disp_mode) {
        return (disp_mode == epoc::display_mode::color16ma) || (disp_mode == epoc::display_mode::color16map);
    }

    int get_bpp_from_display_mode(const epoc::display_mode bpp) {
        switch (bpp) {
        case epoc::display_mode::gray2:
            return 1;
        case epoc::display_mode::gray4:
            return 2;
        case epoc::display_mode::gray16:
        case epoc::display_mode::color16:
            return 4;
        case epoc::display_mode::gray256:
        case epoc::display_mode::color256:
            return 8;
        case epoc::display_mode::color4k:
            return 12;
        case epoc::display_mode::color64k:
            return 16;
        case epoc::display_mode::color16m:
            return 24;
        case epoc::display_mode::color16mu:
        case epoc::display_mode::color16ma:
            return 32;
        }

        return 24;
    }

    int get_num_colors_from_display_mode(const epoc::display_mode disp_mode) {
        switch (disp_mode) {
        case epoc::display_mode::gray2:
            return 2;
        case epoc::display_mode::gray4:
            return 4;
        case epoc::display_mode::gray16:
        case epoc::display_mode::color16:
            return 16;
        case epoc::display_mode::gray256:
        case epoc::display_mode::color256:
            return 256;
        case epoc::display_mode::color4k:
            return 4096;
        case epoc::display_mode::color64k:
            return 65536;
        case epoc::display_mode::color16m:
        case epoc::display_mode::color16mu:
        case epoc::display_mode::color16ma:
        case epoc::display_mode::color16map:
            return 16777216;
        default:
            return 0;
        }
    }

    TKeyCode map_scancode_to_keycode(TStdScanCode scan_code) {
        if (scan_code <= EStdKeyScrollLock) {
            return keymap[scan_code];
        } else if (scan_code > EStdKeyScrollLock && scan_code < EStdKeyF1) {
            return static_cast<TKeyCode>(scan_code);
        } else if (scan_code >= EStdKeyF1 && scan_code <= EStdKeyApplication27) {
            return keymap[scan_code - 67];
        }
        return EKeyNull;
    }

    TStdScanCode map_inputcode_to_scancode(int input_code, int ui_rotation) {
        auto scanmap = &scanmap_0;
        switch (ui_rotation) {
        case 90:
            scanmap = &scanmap_90;
            break;
        case 180:
            scanmap = &scanmap_180;
            break;
        case 270:
            scanmap = &scanmap_270;
            break;
        }

        auto it = scanmap->find(input_code);
        if (it == scanmap->end()) {
            scanmap = &scanmap_all;
            it = scanmap->find(input_code);
            if (it == scanmap->end())
                return EStdKeyNull;
        }
        return static_cast<TStdScanCode>(it->second);
    }
}
