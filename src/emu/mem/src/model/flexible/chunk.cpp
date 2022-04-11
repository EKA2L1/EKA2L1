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

#include <mem/model/flexible/chunk.h>
#include <mem/model/flexible/control.h>
#include <mem/model/flexible/process.h>

#include <common/log.h>

namespace eka2l1::mem::flexible {
    static constexpr vm_address INVALID_ADDR = 0xDEADBEEF;

    flexible_mem_model_chunk::flexible_mem_model_chunk(control_base *control, const asid id)
        : mem_model_chunk(control, id)
        , max_size_(0)
        , committed_(0)
        , flags_(0)
        , fixed_addr_(0)
        , owner_(nullptr) {
    }

    flexible_mem_model_chunk::~flexible_mem_model_chunk() {
    }

    int flexible_mem_model_chunk::do_create(const mem_model_chunk_creation_info &create_info) {
        fixed_addr_ = create_info.addr;
        flags_ = create_info.flags;

        const std::uint32_t total_pages_occupied = static_cast<std::uint32_t>((create_info.size + control_->page_size() - 1) >> control_->page_size_bits_);
        max_size_ = total_pages_occupied << control_->page_size_bits_;
        bottom_ = 0;
        top_ = 0;

        // Create the mem object and page allocator
        mem_obj_ = std::make_unique<memory_object>(control_, total_pages_occupied, create_info.host_map);

        if (!(create_info.flags & MEM_MODEL_CHUNK_TYPE_NORMAL))
            page_bma_ = std::make_unique<common::bitmap_allocator>(total_pages_occupied);

        permission_ = create_info.perm;
        is_addr_shared_ = (create_info.flags & MEM_MODEL_CHUNK_REGION_USER_CODE) || (create_info.flags & MEM_MODEL_CHUNK_REGION_USER_ROM) ||
            (create_info.flags & MEM_MODEL_CHUNK_REGION_KERNEL_MAPPING);

        if (is_addr_shared_) {
            // Treats this as for kernel
            control_flexible *fl_control = reinterpret_cast<control_flexible *>(control_);
            fixed_mapping_ = std::make_unique<mapping>(fl_control->kern_addr_space_.get());
            fixed_mapping_->instantiate(total_pages_occupied, flags_, fixed_addr_);

            // Add this mapping to memobj's mapping
            mem_obj_->attach_mapping(fixed_mapping_.get());
        }

        return 0;
    }

    std::size_t flexible_mem_model_chunk::commit(const vm_address offset, const std::size_t size) { 
        const vm_address dropping_place = static_cast<vm_address>(offset >> control_->page_size_bits_);
        const vm_address dropping_place_end = static_cast<vm_address>((offset + size + control_->page_size() - 1) >> control_->page_size_bits_);

        int total_page_to_commit = static_cast<int>(dropping_place_end - dropping_place);

        // Change the protection of the correspond region in memory object.
        if (!mem_obj_->commit(dropping_place, total_page_to_commit, permission_)) {
            LOG_ERROR(MEMORY, "Unable to commit chunk memory to host!");
            return 0;
        }

        committed_ += (total_page_to_commit - (page_bma_ ? (page_bma_->allocated_count(dropping_place, dropping_place_end - 1)) : 0)) << control_->page_size_bits_;

        // Force fill as allocated
        if (page_bma_) {
            page_bma_->force_fill(dropping_place, total_page_to_commit);
        }

        return total_page_to_commit;
    }

    void flexible_mem_model_chunk::decommit(const vm_address offset, const std::size_t size) {
        const vm_address dropping_place = static_cast<vm_address>(offset >> control_->page_size_bits_);
        const vm_address dropping_place_end = static_cast<vm_address>((offset + size + control_->page_size() - 1) >> control_->page_size_bits_);

        int total_page_to_decommit = static_cast<int>(dropping_place_end - dropping_place);

        // Change the protection of the correspond region in memory object.
        if (!mem_obj_->decommit(dropping_place, total_page_to_decommit)) {
            LOG_ERROR(MEMORY, "Unable to decommit chunk memory from host!");
            return;
        }

        // TODO: This does not seems safe...
        if (page_bma_) {
            page_bma_->deallocate(dropping_place, static_cast<int>(total_page_to_decommit));
        }

        committed_ -= static_cast<std::uint32_t>(total_page_to_decommit << control_->page_size_bits_);
    }

    std::int32_t flexible_mem_model_chunk::allocate(const std::size_t size) {
        const int total_page_to_allocate = static_cast<int>((size + control_->page_size() - 1) >> control_->page_size_bits_);
        int page_allocated = total_page_to_allocate;

        const int page_offset = page_bma_->allocate_from(0, page_allocated);

        if ((page_allocated != total_page_to_allocate) || (page_offset == -1)) {
            LOG_ERROR(MEMORY, "Unable to allocate requested size!");

            if (page_offset != -1) {
                page_bma_->deallocate(page_offset, page_allocated);
            }

            return -2;
        }

        std::uint32_t offset_to_commit_bytes = static_cast<std::uint32_t>(page_offset << control_->page_size_bits_);
        commit(offset_to_commit_bytes, size);

        return static_cast<std::int32_t>(offset_to_commit_bytes);
    }

    void flexible_mem_model_chunk::unmap_from_cpu(mem_model_process *pr, mmu_base *mmu) {
        manipulate_cpu_map(page_bma_.get(), reinterpret_cast<flexible_mem_model_process *>(pr),
            mmu, false);
    }

    void flexible_mem_model_chunk::map_to_cpu(mem_model_process *pr, mmu_base *mmu) {
        manipulate_cpu_map(page_bma_.get(), reinterpret_cast<flexible_mem_model_process *>(pr),
            mmu, true);
    }

    const vm_address flexible_mem_model_chunk::base(mem_model_process *process) {
        if (!process && fixed_addr_) {
            return fixed_addr_;
        }

        if (is_addr_shared_ && fixed_mapping_) {
            return fixed_mapping_->base_;
        }

        if (!process) {
            return INVALID_ADDR;
        }

        // Find our attacher
        flexible_mem_model_process *process_attacher = reinterpret_cast<flexible_mem_model_process *>(process);

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