/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/notifier/plugin.h>
#include <array>
#include <vector>

namespace eka2l1 {
    class kernel_system;

    namespace epoc::notifier {
        constexpr epoc::uid global_message_query_uid = 0x100059CB;

        struct message_query {
            std::int32_t softkeys = 0;
            std::u16string text;
            std::u16string header;
        };

        struct message_query_button {
            std::int16_t command = -1;
            std::u16string text;
        };

        bool read_message_query(std::uint8_t *data, std::size_t size, message_query &query);
        bool read_message_query_buttons(std::uint8_t *data, std::size_t size, std::array<message_query_button, 2> &buttons);
        void add_builtin_plugins(kernel_system *kern, std::vector<plugin_instance> &plugins);
    }
};
