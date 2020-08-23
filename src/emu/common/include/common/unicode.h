/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
 * 
 * Work based of Symbian Open Source.
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
#include <vector>

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

        explicit unicode_comp_state() {
            reset();
        }

        void reset();
        
        inline bool encode_as_is(const std::uint16_t code) {
            return (code == 0x0000) || (code == 0x0009) || (code == 0x000A) || (code == 0x00D) || ((code >= 0x0020) && (code <= 0x007F));
        }

        std::uint32_t dynamic_window_base(int offset_index);
        std::int32_t dynamic_window_offset_index(std::uint16_t code);
        std::int32_t static_window_index(std::uint16_t code);
    };

    struct unicode_stream: public unicode_comp_state {
        int source_pointer;
        std::uint8_t *source_buf;

        int source_size;

        int dest_pointer;
        std::uint8_t *dest_buf;

        int dest_size;
        
        explicit unicode_stream()
            : source_pointer(0)
            , source_size(0)
            , dest_pointer(0)
            , dest_size(0) {
        }

        bool read_byte(std::uint8_t *dat);
        bool read_byte16(std::uint16_t *dat);
        bool write_byte8(std::uint8_t b);
        bool write_byte(std::uint16_t b);
        bool write_byte_be(std::uint16_t b);
        bool write_byte32(std::uint32_t b);
    };

    struct unicode_expander : public unicode_stream {
        bool define_window(const int index);
        bool define_expansion_window();

        bool quote_unicode();

        bool handle_ubyte(const std::uint8_t ubyte);
        bool handle_sbyte(const std::uint8_t sbyte);

        int expand(std::uint8_t *source, int &source_size, std::uint8_t *dest, int dest_size);
    };

    enum unicode_char_treatment {
        unicode_char_treatment_plain_ascii = -2,
        unicode_char_treatment_plain_unicode = -1,
        unicode_char_treatment_first_dynamic = 0,
        unicode_char_treatment_last_dynamic = 255,
        unicode_char_treatment_first_static = 256,
        unicode_char_treatment_last_static = 263
    };

    struct unicode_compressor: public unicode_stream {
        struct action {
            std::uint16_t code_;
            std::int32_t treatment_;

            explicit action(unicode_comp_state &state, const std::uint16_t code_);
        };

        std::int32_t dynamic_window_index_;

        explicit unicode_compressor()
            : dynamic_window_index_(0) {
        }

        std::vector<action> actions_;

        bool write_treatment_selection(const std::int32_t treatment);
        bool write_char(const action &c);
        bool write_uchar(std::uint16_t c);
        bool write_schar(const action &c);

        void write_run();
        int compress(std::uint8_t *source, int &source_size, std::uint8_t *dest, int dest_size);
    };
}