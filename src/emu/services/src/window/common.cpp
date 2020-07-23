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

#include <common/algorithm.h>
#include <common/log.h>
#include <common/time.h>

#include <services/window/common.h>

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
        default:
            return 24;
        }
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

    epoc::display_mode get_display_mode_from_bpp(const int bpp) {
        switch (bpp) {
        case 1:
            return epoc::display_mode::gray2;
        case 2:
            return epoc::display_mode::gray4;
        case 4:
            return epoc::display_mode::color16;
        case 8:
            return epoc::display_mode::color256;
        case 12:
            return epoc::display_mode::color4k;
        case 16:
            return epoc::display_mode::color64k;
        case 24:
            return epoc::display_mode::color16m;
        case 32:
            return epoc::display_mode::color16ma;
        default:
            break;
        }

        return epoc::display_mode::color16m;
    }

    epoc::display_mode string_to_display_mode(const std::string &disp_str) {
        const std::string disp_str_lower = common::lowercase_string(disp_str);
        if (disp_str_lower == "color16map")
            return epoc::display_mode::color16map;
        if (disp_str_lower == "color16ma")
            return epoc::display_mode::color16ma;
        if (disp_str_lower == "color16mu")
            return epoc::display_mode::color16mu;
        if (disp_str_lower == "color64k")
            return epoc::display_mode::color64k;

        LOG_TRACE("Unhandled string to convert to display mode: {}", disp_str);
        return epoc::display_mode::color16ma;
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

    std::uint32_t map_button_to_inputcode(std::map<std::pair<int, int>, std::uint32_t> &map, int controller_id, int button) {
        auto key = std::make_pair(controller_id, button);
        auto it = map.find(key);
        if (it == map.end()) {
            return 0;
        } else {
            return it->second;
        }
    }

    std::uint32_t map_key_to_inputcode(std::map<std::uint32_t, std::uint32_t> &map, std::uint32_t keycode) {
        auto it = map.find(keycode);
        if (it == map.end()) {
            return keycode;
        } else {
            return it->second;
        }
    }
}
