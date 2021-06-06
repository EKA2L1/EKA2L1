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

#include <common/types.h>
#include <cstddef>
#include <cstdint>

namespace eka2l1::mem {
    using vm_address = std::uint32_t;
    using asid = std::int32_t;

    enum class mem_model_type {
        moving,
        multiple,
        flexible
    };

    enum {
        MEM_MODEL_CHUNK_REGION_USER_GLOBAL = 1 << 0,
        MEM_MODEL_CHUNK_REGION_USER_LOCAL = 1 << 1,
        MEM_MODEL_CHUNK_REGION_USER_CODE = 1 << 2,
        MEM_MODEL_CHUNK_REGION_USER_ROM = 1 << 3,
        MEM_MODEL_CHUNK_REGION_KERNEL_MAPPING = 1 << 4,
        MEM_MODEL_CHUNK_REGION_DLL_STATIC_DATA = 1 << 5,
        MEM_MODEL_CHUNK_TYPE_DISCONNECT = 1 << 6,
        MEM_MODEL_CHUNK_TYPE_NORMAL = 1 << 7,
        MEM_MODEL_CHUNK_TYPE_DOUBLE_ENDED = 1 << 8,
        MEM_MODEL_CHUNK_INTERNAL_FORCE_FILL = 1 << 9
    };

    enum {
        MEM_MODEL_CHUNK_ERR_OK = 0,
        MEM_MODEL_CHUNK_ERR_MAXIMUM_CHUNK_OVERFLOW = -1,
        MEM_MODEL_CHUNK_ERR_INVALID_REGION = -2,
        MEM_MODEL_CHUNK_ERR_NO_MEM = -3
    };

    struct mem_model_chunk_creation_info {
        std::size_t size;
        std::uint32_t flags;
        prot perm;

        // Use non-zero value to force the address. Use at your own risk, since this is not checked for overlapping.
        vm_address addr{ 0 };
        void *host_map{ nullptr }; ///< Allow custom host memory mapping to guest.
    };
}
