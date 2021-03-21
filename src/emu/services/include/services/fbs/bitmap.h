/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <loader/mbm.h>
#include <mem/ptr.h>
#include <services/window/common.h>
#include <common/uid.h>

namespace eka2l1 {
    class fbs_server;
}

namespace eka2l1::epoc {
    static constexpr epoc::uid bitwise_bitmap_uid = 0x10000040;
    static constexpr std::uint32_t LEGACY_BMP_COMPRESS_IN_MEMORY_TYPE_BASE = 50;

    enum bitmap_file_compression {
        bitmap_file_no_compression = 0,
        bitmap_file_byte_rle_compression = 1,
        bitmap_file_twelve_bit_rle_compression = 2,
        bitmap_file_sixteen_bit_rle_compression = 3,
        bitmap_file_twenty_four_bit_rle_compression = 4,
        bitmap_file_twenty_four_u_bit_rle_compression = 5,
        bitmap_file_thirty_two_u_bit_rle_compression = 6,
        bitmap_file_thirty_two_a_bit_rle_compression = 7,
        bitmap_file_palette_compression = 8
    };

    enum bitmap_color {
        monochrome_bitmap = 0,
        color_bitmap = 1,
        color_bitmap_with_alpha = 2,
        color_bitmap_with_alpha_pm = 3
    };

    struct bitwise_bitmap {
        enum settings_flag {
            large_bitmap = 0x00010000,
            dirty_bitmap = 0x00010000,
            violate_bitmap = 0x00020000
        };

        uid uid_;

        struct settings {
            // The first 8 bits are reserved for initial display mode
            // The next 8 bits are reserved for current display mode
            // 16 bits left are for flags, on transition mode this is width for some reason lmao
            std::uint32_t flags_{ 0 };

            display_mode initial_display_mode() const;
            display_mode current_display_mode() const;

            void current_display_mode(const display_mode &mode);
            void initial_display_mode(const display_mode &mode);

            bool is_large() const;
            void set_large(const bool result);

            bool dirty_bitmap() const;
            void dirty_bitmap(const bool is_it);

            bool violate_bitmap() const;
            void violate_bitmap(const bool is_it);

            // LEGACY!
            void set_width(const std::uint16_t bpp);
            std::uint16_t get_width() const;
        } settings_;

        eka2l1::ptr<void> allocator_;
        eka2l1::ptr<void> pile_;
        int byte_width_;
        loader::sbm_header header_;
        int spare1_;
        int data_offset_;
        bool compressed_in_ram_;
        bool offset_from_me_;

        void construct(loader::sbm_header &info, epoc::display_mode disp_mode, void *data, const void *base,
            const bool support_current_display_mode_flag, const bool white_fill = false);
        
        void post_construct(fbs_server *serv);
        int copy_to(std::uint8_t *dest, const eka2l1::vec2 &dest_size, fbs_server *serv);

        bitmap_file_compression compression_type() const;

        std::uint8_t *data_pointer(fbs_server *serv);
        std::uint32_t data_size() const;
    };

    bool save_bwbmp_to_file(const std::string &destination, bitwise_bitmap *bitmap, const char *base);
}
