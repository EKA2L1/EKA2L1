/*
 * Copyright (c) 2019 EKA2L1 Team.
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

#include <epoc/mem/mmu.h>

namespace eka2l1::mem {
    constexpr std::size_t PAGE_SIZE_BYTES_10B = 0x1000;
    constexpr std::size_t PAGE_SIZE_BYTES_20B = 0x100000;

    /**
     * \brief Memory management unit.
     */
    class mmu_multiple: public mmu_base {
    public:
        explicit mmu_multiple(page_table_allocator *alloc, const std::size_t psize_bits = 10)
            : mmu_base(alloc, psize_bits) {
        }
    };
}
