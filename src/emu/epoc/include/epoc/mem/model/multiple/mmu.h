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
#include <memory>

namespace eka2l1::mem {
    /**
     * \brief Memory management unit for multiple model.
     */
    class mmu_multiple: public mmu_base {
        std::vector<std::unique_ptr<page_directory>> dirs_;

    public:
        explicit mmu_multiple(page_table_allocator *alloc, const std::size_t psize_bits = 10)
            : mmu_base(alloc, psize_bits) {
        }

        asid rollover_fresh_addr_space() override;
    };
}
