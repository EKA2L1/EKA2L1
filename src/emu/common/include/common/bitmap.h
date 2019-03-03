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

#include <cstddef>
#include <cstdint>

namespace eka2l1::common {
    struct bmp_header {
        std::uint16_t magic { 0x424D };
        std::uint32_t file_size;
        std::uint16_t reserved1;
        std::uint16_t reserved2;
        std::uint32_t pixel_array_offset;
    };

    struct dib_header_core {
        std::uint32_t header_size;
        vec2 size;
        std::uint16_t color_plane_count;
        std::uint16_t bit_per_pixels;

        dib_header_core(const std::uint32_t header_size)
            : header_size(header_size)
            , color_plane_count(0)
            , bit_per_pixels(0) {
        }

        dib_header_core() 
            : header_size(sizeof(dib_header_core))
            , color_plane_count(0)
            , bit_per_pixels(0) {
        }
    };

    struct dib_header_v1 : public dib_header_core {
        std::uint32_t comp;
        std::uint32_t uncompressed_size;
        vec2 print_res;
        std::uint32_t palette_count;
        std::uint32_t important_color_count;

        dib_header_v1() 
            : dib_header_core(sizeof(dib_header_v1))
            , uncompressed_size(0)
            , palette_count(0)
            , important_color_count(0) {
        }
    };
}
