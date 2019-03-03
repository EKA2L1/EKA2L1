/*
 * Copyright (c) 2019 EKA2L1 Team.
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
#include <epoc/utils/uid.h>
#include <cstdint>
#include <vector>

namespace eka2l1::common {
    class ro_buf_stream;
}

namespace eka2l1::loader {
    struct sbm_header {
        std::uint32_t compressed_len;
        std::uint32_t header_len;
        eka2l1::object_size  size_pixels;
        eka2l1::object_size  size_twips;
        std::uint32_t        bit_per_pixels;
        std::uint32_t        color;
        std::uint32_t        palette_size;
        std::uint32_t        compression;
    };

    struct mbm_trailer {
        std::uint32_t              count;
        std::vector<std::uint32_t> sbm_offsets;
    };

    struct mbm_header {
        // UID1 is Direct filestore UID 0x10000037
        // UID2 is Multibitmap UID 0x10000042
        epoc::uid_type uids;
        std::uint32_t uid_checksum;
        std::uint32_t trailer_off;
    };

    struct mbm_file {
        mbm_header header;
        mbm_trailer trailer;

        std::vector<sbm_header> sbm_headers;

        common::ro_buf_stream &stream;

        bool do_read_headers();
        bool valid();

        mbm_file(common::ro_buf_stream &stream)
            : stream(stream) {
        }

        /**
         * \brief Read a bitmap to a buffer at given index in the MBM.
         * 
         * \returns On success, returns true, and dest_max is written back with total
         *          of bytes written in.
         *          On failure, dest_max should contains the uncompress size of the bitmap data. 
         */
        bool read_single_bitmap(const std::size_t index, std::uint8_t *dest, 
            std::size_t &dest_max);
            
        /**

         * \brief Save bitmap at specified index to a file as a BMP file.
         * 
         * \returns True on success
         */
        bool save_bitmap_to_file(const std::size_t index, const char *name);
    };
}
