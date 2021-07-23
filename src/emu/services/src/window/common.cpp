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

    epoc::display_mode get_display_mode_from_bpp(const int bpp, const bool has_color) {
        switch (bpp) {
        case 1:
            return epoc::display_mode::gray2;
        case 2:
            return epoc::display_mode::gray4;
        case 4:
            return (has_color ? epoc::display_mode::color16 : epoc::display_mode::gray16);
        case 8:
            return (has_color ? epoc::display_mode::color256 : epoc::display_mode::gray256);
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

        return epoc::display_mode::color_last;
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

        LOG_TRACE(SERVICE_WINDOW, "Unhandled string to convert to display mode: {}", disp_str);
        return epoc::display_mode::color16ma;
    }

    std::string display_mode_to_string(const epoc::display_mode disp_mode) {
        switch (disp_mode) {
        case epoc::display_mode::color16map:
            return "color16map";

        case epoc::display_mode::color16ma:
            return "color16ma";

        case epoc::display_mode::color16mu:
            return "color16mu";

        case epoc::display_mode::color16m:
            return "color16m";

        case epoc::display_mode::color64k:
            return "color64k";

        case epoc::display_mode::color4k:
            return "color4k";

        case epoc::display_mode::color256:
            return "color256";

        case epoc::display_mode::gray256:
            return "gray256";

        case epoc::display_mode::gray16:
            return "gray16";

        case epoc::display_mode::gray4:
            return "gray4";

        case epoc::display_mode::gray2:
            return "gray2";

        default:
            break;
        }

        return "unk";
    }

    key_code map_scancode_to_keycode(std_scan_code scan_code) {
        if (scan_code <= std_key_scroll_lock) {
            return keymap[scan_code];
        } else if (scan_code > std_key_scroll_lock && scan_code < std_key_f1) {
            return static_cast<key_code>(scan_code);
        } else if (scan_code >= std_key_f1 && scan_code <= std_key_application_27) {
            return keymap[scan_code - 67];
        }
        return key_null;
    }

    std_scan_code post_processing_scancode(std_scan_code resulted, const int rotation) {
        const std_scan_code ROUND_ARROW_MAP[] = {
            std_key_right_arrow,
            std_key_up_arrow,
            std_key_left_arrow,
            std_key_down_arrow
        };

        // Get the index of the key
        int index = -1;
        switch (resulted) {
        case std_key_right_arrow:
            index = 0;
            break;

        case std_key_up_arrow:
            index = 1;
            break;

        case std_key_left_arrow:
            index = 2;
            break;

        case std_key_down_arrow:
            index = 3;
            break;

        default:
            break;
        }

        if (index == -1) {
            return resulted;
        }

        return ROUND_ARROW_MAP[(index + (rotation / 90)) % 4];
    }

    std::optional<std::uint32_t> map_button_to_inputcode(button_map &map, int controller_id, int button) {
        auto key = std::make_pair(controller_id, button);
        auto it = map.find(key);

        if (it == map.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    std::optional<std::uint32_t> map_key_to_inputcode(key_map &map, std::uint32_t keycode) {
        auto it = map.find(keycode);

        if (it == map.end()) {
            return std::nullopt;
        }

        return it->second;
    }
}
