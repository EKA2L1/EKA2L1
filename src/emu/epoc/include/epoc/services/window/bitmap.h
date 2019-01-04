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
        class thread;
    }

    namespace loader {
        struct romimg;
        using romimg_ptr = std::shared_ptr<romimg>;
    }

    using process_ptr = std::shared_ptr<kernel::process>;
    using thread_ptr = std::shared_ptr<kernel::thread>;

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
    
    struct jump_chunk {
        eka2l1::ptr<std::uint32_t> code_ptr;
        std::uint32_t chunk_size;

        bool thumb;
    };

    /* A class manipulates ARM code to do LLE job getting bitmaps related stuff done*/
    class bitmap_manipulator {
        eka2l1::system *sys;

        jump_chunk jump_arm_one_arg;
        jump_chunk jump_thumb_one_arg;

        jump_chunk jump_arm_two_arg;
        jump_chunk jump_thumb_two_arg;

        loader::romimg_ptr        fbscli;

        address duplicate_func_addr;
        address get_data_func_addr;
        address reset_func_addr;

        bool gened = false;

    protected:
        void supply_jump_arg(eka2l1::memory_system *mem, address jump_addr, address r0,
            bool thumb);

        void supply_jump_arg(eka2l1::memory_system *mem, address jump_addr, address r0,
            address r1, bool thumb);
            
        void generate_jump_chunk(eka2l1::memory_system *mem);

        address do_call(eka2l1::process_ptr &call_pr, address jump_addr, std::uint32_t r0);
        address do_call(eka2l1::process_ptr &call_pr, address jump_addr, std::uint32_t r0, std::uint32_t r1);

        // Alloc from the host process
        eka2l1::ptr<fbs_bitmap> scratch;
        fbs_bitmap *scratch_host;

    public:
        explicit bitmap_manipulator(eka2l1::system *sys);

        /*! \brief Get the pointer to the FBS data
            *
            * This invalidate a pre-made code chunk that will returns the address
            * to the bitmap chunk. R0 = this pointer
        */
        eka2l1::ptr<std::uint32_t> get_fbs_data(eka2l1::process_ptr &call_pr,
            eka2l1::ptr<fbs_bitmap> bitmap);
            
        eka2l1::ptr<std::uint32_t> get_fbs_data_with_handle(eka2l1::thread_ptr &call_pr,
            int handle);

        void done_with_fbs_data_from_handle(eka2l1::process_ptr &call_pr);
    };
}