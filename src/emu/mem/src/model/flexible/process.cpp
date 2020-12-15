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

#include <mem/model/flexible/control.h>
#include <mem/model/flexible/process.h>

#include <common/log.h>

namespace eka2l1::mem::flexible {
    const asid flexible_mem_model_process::address_space_id() const {
        return addr_space_->id();
    }

    flexible_mem_model_process::flexible_mem_model_process(control_base *control)
        : mem_model_process(control) {
        addr_space_ = std::make_unique<address_space>(reinterpret_cast<control_flexible*>(control));
    }

    int flexible_mem_model_process::create_chunk(mem_model_chunk *&chunk, const mem_model_chunk_creation_info &create_info) {
        control_flexible *fl_control = reinterpret_cast<control_flexible*>(control_);

        // Allocate a new chunk struct
        flexible_mem_model_chunk *new_chunk_flexible = fl_control->chunk_mngr_->new_chunk(control_, addr_space_->id());

        if (!new_chunk_flexible) {
            LOG_ERROR(MEMORY, "Unable to instantiate new chunk struct!");
            return -1;
        }

        // Set the owner process of this chunk ours! Then Ok, ready to go, construct
        new_chunk_flexible->owner_ = this;
        const int result = new_chunk_flexible->do_create(create_info);

        if (result != 0) {
            // Failed to initialize new chunk, so destroy the pointer allocated for it.
            fl_control->chunk_mngr_->destroy(new_chunk_flexible);
            return result;
        }

        // Attach ourself to the chunk
        if (!attach_chunk(new_chunk_flexible)) {
            // Failed, again free the chunk
            LOG_ERROR(MEMORY, "Failed to attach to newly created chunk!");
            fl_control->chunk_mngr_->destroy(new_chunk_flexible);

            return -1;
        }

        // Pretty much success.
        chunk = reinterpret_cast<mem_model_chunk*>(new_chunk_flexible);
        return 0;
    }

    void flexible_mem_model_process::delete_chunk(mem_model_chunk *chunk) {
        // First, try to detach ourself from this chunk
        if (detach_chunk(chunk)) {
            // Mark this chunk in manager as free
            control_flexible *fl_control = reinterpret_cast<control_flexible*>(control_);
            fl_control->chunk_mngr_->destroy(reinterpret_cast<flexible_mem_model_chunk*>(chunk));
        }
    }

    void *flexible_mem_model_process::get_pointer(const vm_address addr) {
        return addr_space_->dir_->get_pointer(addr);
    }

    bool flexible_mem_model_process::attach_chunk(mem_model_chunk *chunk) {
        // Search in the attached list first if this chunk is here
        auto chunk_ite = std::find_if(attachs_.begin(), attachs_.end(), [chunk](const flexible_mem_model_chunk_attach_info &info) {
            return info.chunk_ == chunk;
        });

        // sniff sniff we may smell bullshit here
        if (chunk_ite != attachs_.end()) {
            // This chunk is already attached
            return false;
        }

        flexible_mem_model_chunk *fl_chunk = reinterpret_cast<flexible_mem_model_chunk*>(chunk);

        // Instantiate new attach info, including new mapping for ourselves
        flexible_mem_model_chunk_attach_info attach_info;
        attach_info.chunk_ = fl_chunk;
        attach_info.map_ = std::make_unique<mapping>(addr_space_.get());

        // Try to instantiate this mapping
        if (!attach_info.map_->instantiate(chunk->max() >> control_->page_size_bits_, fl_chunk->flags_, fl_chunk->fixed_addr_)) {
            LOG_ERROR(MEMORY, "Unable to make new mapping to the address space {}", addr_space_->id());
            return false;
        }

        // We also want to add this to memory object's list of mappings
        fl_chunk->mem_obj_->attach_mapping(attach_info.map_.get());

        // Ok nice nice nice. Add this to list of attachment
        attachs_.push_back(std::move(attach_info));

        return true;
    }

    bool flexible_mem_model_process::detach_chunk(mem_model_chunk *chunk) {
        // Search in the attached list first if this chunk is here
        auto chunk_ite = std::find_if(attachs_.begin(), attachs_.end(), [chunk](const flexible_mem_model_chunk_attach_info &info) {
            return info.chunk_ == chunk;
        });

        if (chunk_ite == attachs_.end()) {
            // This chunk is not attached yet
            return false;
        }

        // Remove the mapping attached to this memory object
        flexible_mem_model_chunk *fl_chunk = reinterpret_cast<flexible_mem_model_chunk*>(chunk);
        fl_chunk->mem_obj_->detach_mapping(chunk_ite->map_.get());

        attachs_.erase(chunk_ite);
        return true;
    }

    static bool should_do_cpu_manipulate(const std::uint32_t flags) {
        return (flags & MEM_MODEL_CHUNK_REGION_USER_LOCAL) || (flags & MEM_MODEL_CHUNK_REGION_USER_GLOBAL)
            || (flags & MEM_MODEL_CHUNK_REGION_DLL_STATIC_DATA) || (flags & MEM_MODEL_CHUNK_REGION_USER_CODE);
    }

    void flexible_mem_model_process::unmap_from_cpu(mmu_base *mmu) {
        for (auto &attached: attachs_) {
            if (should_do_cpu_manipulate(attached.chunk_->flags_)) {
                // This chunk has it address not fixed, so unmap from the CPU
                attached.chunk_->unmap_from_cpu(this, mmu);
            }
        }
    }

    void flexible_mem_model_process::remap_to_cpu(mmu_base *mmu) {
        for (auto &attached: attachs_) {
            if (should_do_cpu_manipulate(attached.chunk_->flags_)) {
                // This chunk has it address not fixed, so map to the CPU
                attached.chunk_->map_to_cpu(this, mmu);
            }
        }
    }
}