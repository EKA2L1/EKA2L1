/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <mem/model/flexible/chunk.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace eka2l1::mem {
    class mmu_base;
}

namespace eka2l1::mem::flexible {
    /**
     * @brief Manager for allocating and destructing chunk struct.
     */
    struct chunk_manager {
        std::vector<std::unique_ptr<flexible_mem_model_chunk>> chunks_;

    public:
        explicit chunk_manager();

        /**
         * @brief       Make a new flexible chunk struct.
         * @returns     Pointer to the chunk on success.
         */
        flexible_mem_model_chunk *new_chunk(mmu_base *mmu, const asid id);

        /**
         * @brief       Destroy an allocated chunk.
         * @param       chunk       Pointer to the chunk to destroy.
         * 
         * @returns     True on success.
         */
        bool destroy(flexible_mem_model_chunk *chunk);
    };
}