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

#include <mem/model/flexible/addrspace.h>
#include <mem/model/flexible/memobj.h>
#include <mem/model/flexible/mapping.h>
#include <mem/mmu.h>

#include <common/algorithm.h>
#include <common/log.h>
#include <common/virtualmem.h>

namespace eka2l1::mem::flexible {
    memory_object::memory_object(mmu_base *mmu, const std::size_t page_count, void *external_host, const std::uint8_t clear_byte)
        : data_(external_host)
        , page_occupied_(page_count)
        , mmu_(mmu)
        , external_(false)
        , clear_byte_(clear_byte) {
        if (data_) {
            external_ = true;
        } else {
            data_ = common::map_memory(page_count * mmu->page_size());

            if (!data_) {
                LOG_ERROR("Unable to allocate virtual memory for this memory object (page count = {})",
                    page_count);
            }
        }
    }

    memory_object::~memory_object() {
        if (data_ && !external_) {
            common::unmap_memory(data_, page_occupied_ * mmu_->page_size());
        }
    }

    bool memory_object::commit(const std::uint32_t page_offset, const std::size_t total_pages, const prot perm) {
        if (page_offset + total_pages > page_occupied_) {
            return false;
        }

        const std::uint32_t start_offset = page_offset << mmu_->page_size_bits_;
        const std::uint32_t size_to_commit = static_cast<std::uint32_t>(total_pages << mmu_->page_size_bits_);

        if (!external_) {
            const bool alloc_result = common::commit(reinterpret_cast<std::uint8_t*>(data_) + start_offset,
                size_to_commit, perm);

            if (!alloc_result) {
                return false;
            }
        }

        // Clear the comitted memory
        std::fill(reinterpret_cast<std::uint8_t*>(data_) + start_offset, reinterpret_cast<std::uint8_t*>(data_) + start_offset + size_to_commit,
            clear_byte_);

        // Map to all mappings
        for (auto &mapping: mappings_) {
            if (!mapping->map(this, page_offset, total_pages, perm)) {
                LOG_WARN("Unable to map committed memory to a mapping!");
            }

            if (mapping->owner_->id() == mmu_->current_addr_space()) {
                // Map it to CPU right away
                mmu_->map_to_cpu(mapping->base_ + start_offset, size_to_commit, reinterpret_cast<std::uint8_t*>(data_) +
                    start_offset, perm);
            }
        }

        return true;
    }

    bool memory_object::decommit(const std::uint32_t page_offset, const std::size_t total_pages) {
        if (page_offset + total_pages > page_occupied_) {
            return false;
        }

        const std::uint32_t start_offset = page_offset << mmu_->page_size_bits_;
        const std::uint32_t size_to_decommit = static_cast<std::uint32_t>(total_pages << mmu_->page_size_bits_);

        if (!external_) {
            const bool deresult = common::decommit(reinterpret_cast<std::uint8_t*>(data_) + start_offset,
                size_to_decommit);

            if (!deresult) {
                return false;
            }
        }

        // Unmap decomitted memory from all mappings
        for (auto &mapping: mappings_) {
            if (!mapping->unmap(page_offset, total_pages)) {
                LOG_WARN("Unable to unmap decommitted memory from a mapping!");
            }
            
            if (mapping->owner_->id() == mmu_->current_addr_space()) {
                // Unmap from to CPU right away
                mmu_->unmap_from_cpu(mapping->base_ + start_offset, size_to_decommit);
            }
        }

        return true;
    }

    bool memory_object::attach_mapping(mapping *layout) {
        if (std::find(mappings_.begin(), mappings_.end(), layout) != mappings_.end())
            return false;

        mappings_.push_back(layout);
        return true;
    }

    bool memory_object::detach_mapping(mapping *layout) {
        auto ite = std::find(mappings_.begin(), mappings_.end(), layout);

        if (ite == mappings_.end()) {
            return false;
        }

        mappings_.erase(ite);
        return true;
    }
}