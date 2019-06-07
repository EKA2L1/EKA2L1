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

#include <cstdint>
#include <vector>

namespace eka2l1::kernel {
    class chunk;
}

namespace eka2l1::epoc {
    enum class akn_skin_chunk_area_base_offset {
        item_def_area_base = 0,
        item_def_area_allocated_size = 1,
        item_def_area_current_size = 2,
        data_area_base = 3,
        data_area_allocated_size = 4,
        data_area_current_size = 5,
        filename_area_base = 6,
        filename_area_allocated_size = 7,
        filename_area_current_size = 8,
        gfx_area_base = 9,
        gfx_area_allocated_size = 10,
        gfx_area_current_size = 11,
        item_def_hash_base = 12,
        item_def_hash_allocated_size = 13,
        item_def_hash_current_size = 14
    };

    class akn_skin_chunk_maintainer {
        struct akn_skin_chunk_area {
            akn_skin_chunk_area_base_offset base_;
            std::size_t gran_off_;
            std::int64_t gran_size_;
        };

        kernel::chunk *shared_chunk_;
        std::size_t current_granularity_off_;
        std::size_t max_size_gran_;

        std::size_t granularity_;

        // Remember to sort this
        std::vector<akn_skin_chunk_area> areas_;

        /**
         * \brief Add a new area in the chunk. 
         *
         * Note: The first chunk size will be subtracted with the chunk header size, as more area will be added.
         * In the case the first area memory will run out if another area got added, this will return false.
         * 
         * \param offset_type             Area type. Must be from enum member "*_base".
         * \param allocated_size_gran     Size to be allocated for the chunk, in granularity.
         *                                If this is negative, the size is equals to granularity divide this.
         * 
         * \returns false if one of these conditions are not met, or insufficient memory.
         */
        bool add_area(const akn_skin_chunk_area_base_offset offset_type, const std::int64_t allocated_size_gran);

    public:
        explicit akn_skin_chunk_maintainer(kernel::chunk *shared_chunk, const std::size_t granularity);
    };
}