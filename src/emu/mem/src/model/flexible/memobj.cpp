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
#include <mem/model/flexible/control.h>
#include <mem/model/flexible/mapping.h>
#include <mem/model/flexible/memobj.h>

#include <common/algorithm.h>
#include <common/log.h>
#include <common/virtualmem.h>

namespace eka2l1::mem::flexible {
    memory_object::memory_object(control_base *ctrl, const std::size_t page_count, void *external_host)
        : data_(external_host)
        , page_occupied_(page_count)
        , control_(ctrl)
        , external_(false)
        , page_arr_(page_count) {
        if (data_) {
            external_ = true;
        } else {
            data_ = common::map_memory(page_count * ctrl->page_size());

            if (!data_) {
                LOG_ERROR(MEMORY, "Unable to allocate virtual memory for this memory object (page count = {})",
                    page_count);
            }
        }
    }

    memory_object::~memory_object() {
        decommit(0, page_occupied_);

        if (data_ && !external_) {
            common::unmap_memory(data_, page_occupied_ * control_->page_size());
        }
    }

    bool memory_object::commit(const std::uint32_t page_offset, const std::size_t total_pages, const prot perm) {
        if (page_offset + total_pages > page_occupied_) {
            return false;
        }

        const std::uint32_t start_offset = page_offset << control_->page_size_bits_;
        const std::uint32_t size_to_commit = static_cast<std::uint32_t>(total_pages << control_->page_size_bits_);

        if (!external_) {
            const bool alloc_result = common::commit(reinterpret_cast<std::uint8_t *>(data_) + start_offset,
                size_to_commit, perm);

            if (!alloc_result) {
                return false;
            }
        }

        control_flexible *ctrl_fx = reinterpret_cast<control_flexible *>(control_);

        // Map to all mappings
        for (auto &mapping : mappings_) {
            if (!mapping->map(this, page_offset, total_pages, perm)) {
                LOG_WARN(MEMORY, "Unable to map committed memory to a mapping!");
            }

            for (auto &mm : ctrl_fx->mmus_) {
                if (mapping->owner_->id() == mm->current_addr_space()) {
                    // Map it to CPU right away
                    mm->map_to_cpu(mapping->base_ + start_offset, size_to_commit, reinterpret_cast<std::uint8_t *>(data_) + start_offset, perm);

                    break;
                }
            }
        }

        page_arr_.alter(page_offset, static_cast<std::uint32_t>(total_pages), perm, false);
        return true;
    }

    bool memory_object::decommit(const std::uint32_t page_offset, const std::size_t total_pages) {
        if (page_offset + total_pages > page_occupied_) {
            return false;
        }

        const std::uint32_t start_offset = page_offset << control_->page_size_bits_;
        const std::uint32_t size_to_decommit = static_cast<std::uint32_t>(total_pages << control_->page_size_bits_);

        control_flexible *ctrl_fx = reinterpret_cast<control_flexible *>(control_);

        // Remove guest mappings and cached host translations before making the
        // backing inaccessible. This matches the multiple memory model and
        // prevents a concurrent CPU fast-path access from reaching a page after
        // common::decommit has reclaimed it.
        for (auto &mapping : mappings_) {
            if (!mapping->unmap(page_offset, total_pages)) {
                LOG_WARN(MEMORY, "Unable to unmap decommitted memory from a mapping!");
            }

            // Invalidate the CPU TLB for every mapping, not just the one owned by the
            // current address space: kernel/shared fixed mappings (code, ROM) are visible
            // from every address space, so the running core may hold entries for them
            // even while another address space is current, and the host memory is about
            // to be freed. Dirtying a foreign or stale entry is always safe.
            for (auto &mm : ctrl_fx->mmus_) {
                mm->unmap_from_cpu(mapping->base_ + start_offset, size_to_decommit);
            }
        }

        page_arr_.alter(page_offset, static_cast<std::uint32_t>(total_pages), prot_none, true);

        if (!external_) {
            const bool deresult = common::decommit(reinterpret_cast<std::uint8_t *>(data_) + start_offset,
                size_to_decommit);

            if (!deresult) {
                return false;
            }
        }

        return true;
    }

    bool memory_object::attach_mapping(mapping *layout) {
        if (std::find(mappings_.begin(), mappings_.end(), layout) != mappings_.end())
            return false;

        mappings_.push_back(layout);
        page_arr_.supply_mapping(this, layout);

        return true;
    }

    bool memory_object::detach_mapping(mapping *layout) {
        auto ite = std::find(mappings_.begin(), mappings_.end(), layout);

        if (ite == mappings_.end()) {
            return false;
        }

        // Chunk/process teardown detaches mappings before destroying their
        // memory object. If detach merely erases the pointer, decommit() can no
        // longer find this virtual range and the CPU TLB keeps host pointers
        // past the backing allocation's lifetime. Clear the page tables first
        // so a miss cannot repopulate the entry, then invalidate the full
        // mapping on every core. mapping::~mapping() repeats unmap(), which is
        // intentionally harmless.
        if (!layout->unmap(0, layout->occupied_)) {
            LOG_WARN(MEMORY, "Unable to unmap detached memory mapping!");
        }

        control_flexible *ctrl_fx = reinterpret_cast<control_flexible *>(control_);
        const std::size_t mapping_size = layout->occupied_ << control_->page_size_bits_;
        for (auto &mm : ctrl_fx->mmus_) {
            mm->unmap_from_cpu(layout->base_, mapping_size);
        }

        mappings_.erase(ite);
        return true;
    }
}
