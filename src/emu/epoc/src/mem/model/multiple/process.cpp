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

#include <epoc/mem/model/multiple/chunk.h>
#include <epoc/mem/model/multiple/mmu.h>
#include <epoc/mem/model/multiple/process.h>

#include <epoc/mem/mmu.h>
#include <common/virtualmem.h>

namespace eka2l1::mem {
    multiple_mem_model_process::multiple_mem_model_process(mmu_base *mmu) 
        : mem_model_process(mmu)
        , addr_space_id_(mmu->rollover_fresh_addr_space())
        , user_local_sec_(local_data, shared_data, mmu->page_size()) {
    }

    static constexpr std::size_t MAX_CHUNK_ALLOW_PER_PROCESS = 512;

    multiple_mem_model_chunk *multiple_mem_model_process::allocate_chunk_struct_ptr() {
        if (chunks_.size() >= MAX_CHUNK_ALLOW_PER_PROCESS) {
            return nullptr;
        }

        // Iterate and search for free slot
        for (std::size_t i = 0; i < chunks_.size(); i++) {
            if (!chunks_[i]) {
                chunks_[i] = std::make_unique<multiple_mem_model_chunk>(mmu_, addr_space_id_);

                chunks_[i] ->chunk_id_in_mmp_ = i + 1;
                chunks_[i] ->own_process_ = this;

                return chunks_[i].get();
            }
        }

        chunks_.push_back(std::make_unique<multiple_mem_model_chunk>(mmu_, addr_space_id_));

        chunks_.back()->chunk_id_in_mmp_ = chunks_.size();
        chunks_.back()->own_process_ = this;

        return chunks_.back().get();
    }

    linear_section *multiple_mem_model_process::get_section(const std::uint32_t flags) {
        mmu_multiple *mul_mmu = reinterpret_cast<mmu_multiple*>(mmu_); 
        
        if (flags & MEM_MODEL_CHUNK_REGION_USER_CODE) {
            return &mul_mmu->user_code_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_USER_GLOBAL) {
            return &mul_mmu->user_global_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_USER_LOCAL) {
            return &user_local_sec_;
        }

        return nullptr;
    }

    void *multiple_mem_model_process::get_pointer(const vm_address addr) {
        return mmu_->get_host_pointer(addr_space_id_, addr);
    }
    
    int multiple_mem_model_process::create_chunk(mem_model_chunk *&chunk, const mem_model_chunk_creation_info &create_info) {
        // Allocate free chunk pointer first
        multiple_mem_model_chunk *mchunk = allocate_chunk_struct_ptr();

        if (mchunk == nullptr) {
            return MEM_MODEL_CHUNK_ERR_MAXIMUM_CHUNK_OVERFLOW;
        }

        // get the right virtual address space allocator
        linear_section *chunk_sec = get_section(create_info.flags);

        if (!chunk_sec) {
            // No allocator suitable for the given flags
            return MEM_MODEL_CHUNK_ERR_INVALID_REGION;
        }

        // Allocate virtual pages
        const int offset = chunk_sec->alloc_.allocate_from(0, static_cast<int>(create_info.size / mmu_->page_size()),
            true);

        if (offset == -1) {
            return MEM_MODEL_CHUNK_ERR_NO_MEM;
        }

        const vm_address addr = (offset >> mmu_->page_size_bits_) + chunk_sec->beg_;
        mchunk->base_ = addr;
        mchunk->committed_ = 0;

        // Calculate total pages
        const std::uint32_t total_pages = static_cast<std::uint32_t>(
            (create_info.size + mmu_->page_size() - 1) >> mmu_->page_size_bits_);

        mchunk->max_size_ = total_pages << mmu_->page_size_bits_;

        // Map host base memory
        mchunk->host_base_ = common::map_memory(mchunk->max_size_);

        if (!mchunk->host_base_) {
            return MEM_MODEL_CHUNK_ERR_NO_MEM;
        }
        
        // Allocate page table IDs storage that the chunk will use
        mchunk->page_tabs_.resize((create_info.size + mmu_->chunk_mask_ - 1) >> 
            mmu_->chunk_shift_);

        mchunk->permission_ = create_info.perm;

        return MEM_MODEL_CHUNK_ERR_OK;
    }

    void multiple_mem_model_process::delete_chunk(mem_model_chunk *chunk) {

    }
}
