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

#include <mem/model/multiple/chunk.h>
#include <mem/model/multiple/mmu.h>
#include <mem/model/multiple/process.h>

#include <common/algorithm.h>
#include <common/virtualmem.h>
#include <mem/mmu.h>

#include <arm/arm_interface.h>

namespace eka2l1::mem {
    multiple_mem_model_process::multiple_mem_model_process(mmu_base *mmu)
        : mem_model_process(mmu)
        , addr_space_id_(mmu->rollover_fresh_addr_space())
        , user_local_sec_(local_data, shared_data, mmu->page_size())
        , user_dll_static_data_sec_(dll_static_data, shared_data, mmu->page_size()) {
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

                chunks_[i]->chunk_id_in_mmp_ = i + 1;
                chunks_[i]->own_process_ = this;

                return chunks_[i].get();
            }
        }

        chunks_.push_back(std::make_unique<multiple_mem_model_chunk>(mmu_, addr_space_id_));

        chunks_.back()->chunk_id_in_mmp_ = chunks_.size();
        chunks_.back()->own_process_ = this;

        return chunks_.back().get();
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

        mchunk->own_process_ = this;
        mchunk->granularity_shift_ = mmu_->chunk_shift_;

        chunk = reinterpret_cast<mem_model_chunk *>(mchunk);

        return mchunk->do_create(create_info);
    }

    void multiple_mem_model_process::delete_chunk(mem_model_chunk *chunk) {
        multiple_mem_model_chunk *mul_chunk = reinterpret_cast<multiple_mem_model_chunk *>(chunk);

        // Decommit the whole things
        mul_chunk->decommit(0, mul_chunk->max_size_);

        // Ignore the result, just unmap things
        if (!mul_chunk->is_external_host)
            common::unmap_memory(mul_chunk->host_base_, mul_chunk->max_size_);

        for (std::size_t i = 0; i < chunks_.size(); i++) {
            if (chunks_[i].get() == mul_chunk) {
                chunks_[i].reset();
            }
        }
    }

    bool multiple_mem_model_process::attach_chunk(mem_model_chunk *chunk) {
        multiple_mem_model_chunk *mul_chunk = reinterpret_cast<multiple_mem_model_chunk *>(chunk);

        if (mul_chunk->is_local && mul_chunk->own_process_ != this) {
            return false;
        } else {
            // Already attach it
            return true;
        }

        if (std::find(attached_.begin(), attached_.end(), mul_chunk) == attached_.end()) {
            return false;
        }

        attached_.push_back(mul_chunk);

        // Add our address ID space to the chunk
        mul_chunk->attached_asids_.push_back(addr_space_id_);

        // Assign page tables
        for (std::size_t i = 0; i < mul_chunk->page_tabs_.size(); i++) {
            if (mul_chunk->page_tabs_[i] != 0xFFFFFFFF) {
                mmu_->assign_page_table(mmu_->get_page_table_by_id(mul_chunk->page_tabs_[i]),
                    static_cast<vm_address>(mul_chunk->base_ + (i << mmu_->page_size_bits_)),
                    0, &addr_space_id_, 1);
            }
        }

        return true;
    }

    bool multiple_mem_model_process::detach_chunk(mem_model_chunk *chunk) {
        multiple_mem_model_chunk *mul_chunk = reinterpret_cast<multiple_mem_model_chunk *>(chunk);

        // Delete our address space ID attached in the chunk
        auto result = std::find(mul_chunk->attached_asids_.begin(), mul_chunk->attached_asids_.end(),
            addr_space_id_);

        if (result == mul_chunk->attached_asids_.end()) {
            // Our id not found
            return false;
        }

        attached_.erase(std::find(attached_.begin(), attached_.end(), mul_chunk));

        // Remove it
        mul_chunk->attached_asids_.erase(result);

        // Unassign page tables
        for (std::size_t i = 0; i < mul_chunk->page_tabs_.size(); i++) {
            if (mul_chunk->page_tabs_[i] != 0xFFFFFFFF) {
                mmu_->assign_page_table(nullptr, static_cast<vm_address>(mul_chunk->base_ + (i << mmu_->page_size_bits_)),
                    0, &addr_space_id_, 1);
            }
        }

        return true;
    }

    void multiple_mem_model_process::unmap_from_cpu() {
        if (!mmu_->cpu_->should_clear_old_memory_map()) {
            return;
        }

        for (auto &c : chunks_) {
            if (c && c->is_local) {
                // Local
                c->unmap_from_cpu(this);
            }
        }
    }

    void multiple_mem_model_process::remap_to_cpu() {
        for (auto &c : chunks_) {
            if (c && c->is_local) {
                // Local
                c->map_to_cpu(this);
            }
        }
    }
}
