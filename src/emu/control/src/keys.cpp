/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#include <control/keys.h>

#include <services/window/keys.h>

#include <array>
#include <utility>

namespace eka2l1::control {
    std::optional<std::uint32_t> scan_code_of_key(std::string_view name) {
        // The S60 keypad. Digits, '*' and '#' follow, then any other key is sent by scan code.
        static constexpr std::array<std::pair<std::string_view, std::uint32_t>, 13> NAMED_KEYS = { {
            { "left_softkey", epoc::std_key_device_0 },
            { "right_softkey", epoc::std_key_device_1 },
            { "select", epoc::std_key_device_3 },
            { "up", epoc::std_key_up_arrow },
            { "down", epoc::std_key_down_arrow },
            { "left", epoc::std_key_left_arrow },
            { "right", epoc::std_key_right_arrow },
            { "send", epoc::std_key_yes },
            { "end", epoc::std_key_no },
            { "menu", epoc::std_key_application_0 },
            { "edit", epoc::std_key_left_shift },
            { "clear", epoc::std_key_backspace },
            { "hash", epoc::std_key_hash },
        } };

        for (const auto &[key_name, scan_code] : NAMED_KEYS) {
            if (key_name == name) {
                return scan_code;
            }
        }

        if (name == "star") {
            return static_cast<std::uint32_t>('*');
        }

        if ((name.size() == 1) && (name[0] >= '0') && (name[0] <= '9')) {
            return static_cast<std::uint32_t>(name[0]);
        }

        return std::nullopt;
    }
}
