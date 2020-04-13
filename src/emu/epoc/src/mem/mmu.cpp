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

#include <arm/arm_interface.h>
#include <epoc/mem/mmu.h>

#include <epoc/mem/model/multiple/mmu.h>

namespace eka2l1::mem {
    mmu_base::mmu_base(page_table_allocator *alloc, arm::arm_interface *cpu, const std::size_t psize_bits, const bool mem_map_old)
        : alloc_(alloc)
        , cpu_(cpu)
        , page_size_bits_(psize_bits)
        , mem_map_old_(mem_map_old) {
        if (psize_bits == 20) {
            offset_mask_ = OFFSET_MASK_20B;
            page_table_index_shift_ = PAGE_TABLE_INDEX_SHIFT_20B;
            page_index_mask_ = PAGE_INDEX_MASK_20B;
            page_index_shift_ = PAGE_INDEX_SHIFT_20B;
            chunk_shift_ = CHUNK_SHIFT_20B;
            chunk_mask_ = CHUNK_MASK_20B;
            chunk_size_ = CHUNK_SIZE_20B;
            page_per_tab_shift_ = PAGE_PER_TABLE_SHIFT_20B;
        } else {
            offset_mask_ = OFFSET_MASK_12B;
            page_table_index_shift_ = PAGE_TABLE_INDEX_SHIFT_12B;
            page_index_mask_ = PAGE_INDEX_MASK_12B;
            page_index_shift_ = PAGE_INDEX_SHIFT_12B;
            chunk_shift_ = CHUNK_SHIFT_12B;
            chunk_mask_ = CHUNK_MASK_12B;
            chunk_size_ = CHUNK_SIZE_12B;
            page_per_tab_shift_ = PAGE_PER_TABLE_SHIFT_12B;
        }
    }

    page_table *mmu_base::create_new_page_table() {
        return alloc_->create_new(page_size_bits_);
    }

    void mmu_base::map_to_cpu(const vm_address addr, const std::size_t size, void *ptr, const prot perm) {
        cpu_->map_backing_mem(addr, size, reinterpret_cast<std::uint8_t *>(ptr), perm);
    }

    void mmu_base::unmap_from_cpu(const vm_address addr, const std::size_t size) {
        cpu_->unmap_memory(addr, size);
    }

    mmu_impl make_new_mmu(page_table_allocator *alloc, arm::arm_interface *cpu, const std::size_t psize_bits, const bool mem_map_old,
        const mem_model_type model) {
        switch (model) {
        case mem_model_type::multiple: {
            return std::make_unique<mmu_multiple>(alloc, cpu, psize_bits, mem_map_old);
        }

        default:
            break;
        }

        return nullptr;
    }
}
