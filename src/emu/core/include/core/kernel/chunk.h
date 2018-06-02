/*
 * Copyright (c) 2018 EKA2L1 Team.
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
#include <kernel/kernel_obj.h>
#include <ptr.h>

#include <cstdint>
#include <string>

namespace eka2l1 {
    using address = uint32_t;
    
    class memory_system;
    class kernel_system;

    namespace kernel {
        enum class chunk_type {
            disconnected,
            double_ended,
            normal
        };

        enum class chunk_access {
            global,
            local,
            code
        };

        enum class chunk_attrib {
            none,
            anonymous
        };

        class chunk: public kernel_obj {
            // The reversed region that the chunk can commit to
            address top;
            address bottom;

            prot   protection;
            size_t max_size;

            chunk_type type;
            chunk_access access;
            chunk_attrib attrib;

            std::string name;

            eka2l1::ptr<uint8_t> chunk_base;

            kernel_system* kern;
            memory_system* mem;

        public:
            // The top and bottom address will be rounded up to align the page size
            chunk(kernel_system* kern, memory_system* mem, std::string name, address top, const address bottom, const size_t max_grow_size, prot protection,
                chunk_type type, chunk_access access, chunk_attrib attrib);

            ~chunk();

            // Commit to a disconnected chunk. Offset and size SHOULD be aligned with the page size
            // Else this will results unwanted behavior. E.g commit(page_size - 1, 2), should commit both
            // the first and second page, since the offset is at the first page, and the commit contains
            // both last and first byte of two page. This will results two WHOLE pages being allocated. 
            void commit(uint32_t offset, size_t size);

            // Decommit to a disconnected chunk. Offset and size SHOULD be aligned with the page size
            // The reason is same as for commit
            void decommit(uint32_t offset, size_t size);

            chunk_type get_chunk_type() const {
                return type;
            }

            ptr<uint8_t> base() {
                return chunk_base;
            }
        };
    }
}
