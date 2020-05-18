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

#include <epoc/mem/chunk.h>
#include <epoc/mem/mmu.h>
#include <epoc/mem/model/flexible/chunk.h>
#include <epoc/mem/model/multiple/chunk.h>

#include <common/algorithm.h>
#include <common/allocator.h>
#include <common/log.h>

namespace eka2l1::mem {
    const vm_address mem_model_chunk::bottom() const {
        return bottom_ << mmu_->page_size_bits_;
    }

    const vm_address mem_model_chunk::top() const {
        return top_ << mmu_->page_size_bits_;
    }
    
    bool mem_model_chunk::adjust(const vm_address bottom, const vm_address top) {
        const std::size_t top_page_off = ((top + mmu_->page_size() - 1) >> mmu_->page_size_bits_);
        const std::size_t bottom_page_off = (bottom >> mmu_->page_size_bits_);

        // Check the top
        // Top offset adjusted smaller than current top offset
        if (top_page_off < top_) {
            // Decommit
            decommit(static_cast<vm_address>(top_page_off << mmu_->page_size_bits_),
                (top_ - top_page_off) << mmu_->page_size_bits_);
        } else if (top_page_off > top_) {
            // We must commit more memory
            commit(static_cast<vm_address>(top_ << mmu_->page_size_bits_),
                (top_page_off - top_) << mmu_->page_size_bits_);
        }

        top_ = static_cast<vm_address>(top_page_off);

        // Adjust double ended
        if (bottom != 0xFFFFFFFF) {
            // Check the bottom
            if (bottom_page_off > bottom_) {
                // Decommit
                decommit(static_cast<vm_address>(bottom_ << mmu_->page_size_bits_),
                    (bottom_page_off - bottom_) << mmu_->page_size_bits_);
            } else if (bottom_page_off < bottom_) {
                // We must commit more memory
                commit(static_cast<vm_address>(bottom_page_off << mmu_->page_size_bits_),
                    (bottom_ - bottom_page_off) << mmu_->page_size_bits_);
            }

            bottom_ = static_cast<vm_address>(bottom_page_off);
        }

        return true;
    }

    void mem_model_chunk::manipulate_cpu_map(common::bitmap_allocator *allocator, mem_model_process *process,
        const bool map) {
        // Get the base address for this process
        const vm_address base_addr = base(process);
        
        if (!base_addr) {
            LOG_ERROR("Unable to get base address of this chunk!");
            return;
        }

        auto do_the_map = [&](const std::uint32_t start_index, const std::uint32_t page_count) {
            if (map) {
                mmu_->map_to_cpu(base_addr + (start_index << mmu_->page_size_bits_), page_count << mmu_->page_size_bits_,
                    reinterpret_cast<std::uint8_t *>(host_base()) + (start_index << mmu_->page_size_bits_), permission_);
            } else {
                mmu_->unmap_from_cpu(base_addr + (start_index << mmu_->page_size_bits_), page_count << mmu_->page_size_bits_);
            }
        };

        if (!allocator) {
            // Contigious types. Just unmap/map directly
            do_the_map(bottom_, top_ - bottom_);
            return;
        }

        // Iterate through all blocks
        for (std::size_t i = 0; i < allocator->words_.size(); i++) {
            if (allocator->words_[i] == 0) {
                continue;
            }

            if (allocator->words_[i] == 0xFFFFFFFF) {
                // Map all
                do_the_map(static_cast<std::uint32_t>(i << 5), 32);
                continue;
            }

            std::uint32_t offset_beg = 0;
            std::uint32_t offset_end = common::find_most_significant_bit_one(allocator->words_[i]);

            for (offset_beg; offset_beg <= offset_end; offset_beg++) {
                if (allocator->words_[i] & (1 << offset_beg)) {
                    std::uint32_t offset_beg_committed = offset_beg;

                    while ((allocator->words_[i] & (1 << offset_beg))) {
                        offset_beg++;
                    }

                    do_the_map(static_cast<std::uint32_t>((i << 5) + offset_beg_committed),
                        static_cast<std::uint32_t>(offset_beg - offset_beg_committed));
                }
            }
        }
    }
    
    mem_model_chunk_impl make_new_mem_model_chunk(mmu_base *mmu, const asid addr_space_id,
        const mem_model_type mmt) {
        switch (mmt) {
        case mem_model_type::multiple: {
            return std::make_unique<multiple_mem_model_chunk>(mmu, addr_space_id);
        }

        case mem_model_type::flexible:
            return std::make_unique<flexible::flexible_mem_model_chunk>(mmu, addr_space_id);

        default:
            break;
        }

        return nullptr;
    }
}
