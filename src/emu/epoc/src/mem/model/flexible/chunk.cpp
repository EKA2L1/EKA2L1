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

#include <epoc/mem/model/flexible/chunk.h>
#include <epoc/mem/model/flexible/mmu.h>
#include <epoc/mem/model/flexible/process.h>

#include <common/log.h>

namespace eka2l1::mem::flexible {
    static constexpr vm_address INVALID_ADDR = 0xDEADBEEF;

    flexible_mem_model_chunk::flexible_mem_model_chunk(mmu_base *mmu, const asid id)
        : mem_model_chunk(mmu, id) {
    }

    flexible_mem_model_chunk::~flexible_mem_model_chunk() {
    }

    int flexible_mem_model_chunk::do_create(const mem_model_chunk_creation_info &create_info) {
        fixed_ = false;
        flags_ = create_info.flags;

        if ((create_info.flags & MEM_MODEL_CHUNK_REGION_USER_LOCAL) ||
            (create_info.flags & MEM_MODEL_CHUNK_REGION_USER_ROM) ||
            (create_info.flags & MEM_MODEL_CHUNK_REGION_ROM_BSS)) {
            // These has address after instantiate fixed, mark it as so
            fixed_ = true;
        }

        const std::uint32_t total_pages_occupied = static_cast<std::uint32_t>((create_info.size +
            mmu_->page_size() - 1) >> mmu_->page_size_bits_);
        max_size_ = total_pages_occupied << mmu_->page_size_bits_;
        bottom_ = 0;
        top_ = 0;

        // Create the mem object and page allocator
        mem_obj_ = std::make_unique<memory_object>(mmu_, total_pages_occupied, create_info.host_map);
        
        if (!(create_info.flags & MEM_MODEL_CHUNK_TYPE_NORMAL))
            page_bma_ = std::make_unique<common::bitmap_allocator>(total_pages_occupied);

        permission_ = create_info.perm;

        return 0;
    }

    std::size_t flexible_mem_model_chunk::commit(const vm_address offset, const std::size_t size) {
        int total_page_to_commit = static_cast<int>((size + mmu_->page_size() - 1) >> mmu_->page_size_bits_);
        const vm_address dropping_place = static_cast<vm_address>(offset >> mmu_->page_size_bits_);

        // Change the protection of the correspond region in memory object.
        if (!mem_obj_->commit(dropping_place, total_page_to_commit, permission_)) {
            LOG_ERROR("Unable to commit chunk memory to host!");
            return 0;
        }

        committed_ += (total_page_to_commit - (page_bma_->allocated_count(dropping_place,
            dropping_place + total_page_to_commit - 1))) << mmu_->page_size_bits_;

        // Force fill as allocated
        page_bma_->force_fill(dropping_place, total_page_to_commit);
        return total_page_to_commit;
    }

    void flexible_mem_model_chunk::decommit(const vm_address offset, const std::size_t size) {
        const std::size_t total_page_to_decommit = (size + mmu_->page_size() - 1) >> mmu_->page_size_bits_;
        const vm_address dropping_place = static_cast<vm_address>(offset >> mmu_->page_size_bits_);

        // Change the protection of the correspond region in memory object.
        if (!mem_obj_->decommit(dropping_place, total_page_to_decommit)) {
            LOG_ERROR("Unable to decommit chunk memory from host!");
            return;
        }

        // TODO: This does not seems safe...
        page_bma_->free(dropping_place, static_cast<int>(total_page_to_decommit));
        committed_ -= static_cast<std::uint32_t>(total_page_to_decommit << mmu_->page_size_bits_);
    }

    bool flexible_mem_model_chunk::allocate(const std::size_t size) {
        const int total_page_to_allocate = static_cast<int>((size + mmu_->page_size() - 1) >> mmu_->page_size_bits_);
        int page_allocated = total_page_to_allocate;
        
        const int page_offset = page_bma_->allocate_from(0, page_allocated);

        if ((page_allocated != total_page_to_allocate) || (page_offset == -1)) {
            LOG_ERROR("Unable to allocate requested size!");

            if (page_offset != -1) {
                page_bma_->free(page_offset, page_allocated);
            }

            return false;
        }

        commit(page_offset << mmu_->page_size_bits_, size);
        return true;
    }

    void flexible_mem_model_chunk::unmap_from_cpu(mem_model_process *pr) {
        manipulate_cpu_map(page_bma_.get(), reinterpret_cast<flexible_mem_model_process*>(pr), false);
    }

    void flexible_mem_model_chunk::map_to_cpu(mem_model_process *pr) {
        manipulate_cpu_map(page_bma_.get(), reinterpret_cast<flexible_mem_model_process*>(pr), true);
    }

    const vm_address flexible_mem_model_chunk::base(mem_model_process *process) {
        // Find our attacher
        flexible_mem_model_process *process_attacher = reinterpret_cast<flexible_mem_model_process*>(process);

        // Find our chunks in the process
        auto info_result = std::find_if(process_attacher->attachs_.begin(), process_attacher->attachs_.end(),
            [this](const flexible_mem_model_chunk_attach_info &info) {
                return info.chunk_ == this;
            });

        if (info_result == process_attacher->attachs_.end()) {
            return INVALID_ADDR;
        }

        return info_result->map_->base_;
    }

    void *flexible_mem_model_chunk::host_base() {
        return mem_obj_->ptr();
    }
}