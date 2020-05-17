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

#pragma once

#include <common/types.h>
#include <epoc/mem/common.h>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace eka2l1::mem {
    struct mem_model_chunk;
    class mmu_base;

    struct mem_model_process;

    /**
     * \brief A component in process implementation, that provides memory manipulation of process address space.
     */
    struct mem_model_process {
        mmu_base *mmu_;

    public:
        explicit mem_model_process(mmu_base *mmu)
            : mmu_(mmu) {
        }

        virtual ~mem_model_process() {
        }

        virtual const asid address_space_id() const = 0;

        virtual int create_chunk(mem_model_chunk *&chunk, const mem_model_chunk_creation_info &create_info) = 0;
        virtual void delete_chunk(mem_model_chunk *chunk) = 0;

        virtual void *get_pointer(const vm_address addr) = 0;

        virtual bool attach_chunk(mem_model_chunk *chunk) = 0;
        virtual bool detach_chunk(mem_model_chunk *chunk) = 0;

        virtual void unmap_from_cpu() = 0;
        virtual void remap_to_cpu() = 0;
    };

    using mem_model_process_impl = std::unique_ptr<mem_model_process>;

    mem_model_process_impl make_new_mem_model_process(mmu_base *mmu, const mem_model_type model);
}
