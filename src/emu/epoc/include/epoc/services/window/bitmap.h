/*
 * Copyright (c) 2018 EKA2L1 Team
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

#include <common/types.h>
#include <common/vecx.h>

#include <epoc/ptr.h>
#include <memory>

namespace eka2l1 {
    class system;

    namespace kernel {
        class process;
    }

    using process_ptr = std::shared_ptr<kernel::process>;

    enum class compression_scheme {
        rle_compression,
        palette_compression,
        palette_with_rle_fallback
    };

    struct bitwise_bitmap {
        std::uint32_t uid;
        std::uint32_t setting_data;

        eka2l1::ptr<void> rheap;
        eka2l1::ptr<void> chunk_pile;

        int byte_width;

        int bitmap_size;
        int struct_size;

        eka2l1::vec2 size_pixels;
        eka2l1::vec2 size_twips;

        int bits_per_pixel;
        int pattele_entries;

        compression_scheme scheme;

        int spare;
        int data_offset;

        bool is_compressed_in_ram;
    };

    // We need to combine native ARM code + hle to get the address of raw data
    struct fbs_bitmap {
        address vtable;

        eka2l1::ptr<void> fbs_session;
        eka2l1::ptr<void> bitwise_bitmap;

        std::uint16_t flags;
        std::uint16_t use_count;

        int handle;
        int server_handle;
    };
    
    /*! \brief Get the pointer to the FBS data
        *
        * This invalidate a pre-made code chunk that will returns the address
        * to the bitmap chunk. R0 = this pointer
    */
    eka2l1::ptr<std::uint32_t> get_fbs_data(eka2l1::system *sys, eka2l1::process_ptr call_pr,
        eka2l1::ptr<fbs_bitmap> bitmap);
}