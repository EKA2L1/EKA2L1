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

#pragma once

#include <cstdint>

namespace eka2l1::common {
    struct unicode_comp_state {
        enum {
            static_windows_size = 8,
            dynamic_windows_size = 8,
            special_base_size = 7
        };

        bool unicode_mode;
        std::uint32_t active_window_base;

        std::uint32_t static_windows[static_windows_size];
        std::uint32_t dynamic_windows[dynamic_windows_size];
        std::uint16_t special_bases[special_base_size];
        
        void reset();

        std::uint32_t dynamic_window_base(int offset_index);
    }; 

    struct unicode_expander: public unicode_comp_state {
        int source_pointer;
        std::uint8_t *source_buf;

        int source_size;

        int dest_pointer;
        std::uint8_t *dest_buf;
        
        int dest_size;

        unicode_expander()
            : source_pointer(0), source_size(0), dest_pointer(0), dest_size(0) {
            reset();
        }

        bool read_byte(std::uint8_t *dat);
        bool write_byte8(std::uint8_t b);
        bool write_byte(std::uint16_t b);
        bool write_byte32(std::uint32_t b);

        bool define_window(const int index);
        bool define_expansion_window();

        bool quote_unicode();

        bool handle_ubyte(const std::uint8_t ubyte);
        bool handle_sbyte(const std::uint8_t sbyte);

        int expand(std::uint8_t *source, int source_size,
            std::uint8_t *dest, int dest_size);
    };
}