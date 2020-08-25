/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <services/allocator.h>
#include <kernel/chunk.h>

namespace eka2l1::epoc {
    chunk_allocator::chunk_allocator(chunk_ptr de_chunk)
        : block_allocator(reinterpret_cast<std::uint8_t*>(de_chunk->host_base()), de_chunk->committed())
        , target_chunk(std::move(de_chunk)) {
    }

    bool chunk_allocator::expand(std::size_t target) {
        return target_chunk->adjust(target);
    }

    address chunk_allocator::to_address(const void *addr, kernel::process *pr) {
        return static_cast<address>(reinterpret_cast<const std::uint8_t*>(addr) - reinterpret_cast<const std::uint8_t*>(
            target_chunk->host_base())) + target_chunk->base(pr).ptr_address();
    }

    void *chunk_allocator::to_pointer(const address addr, kernel::process *pr) {
        return reinterpret_cast<std::uint8_t*>(target_chunk->host_base()) + (addr -target_chunk->base(pr).ptr_address());
    }
}