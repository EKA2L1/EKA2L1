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

#pragma once

#include <mem/model/flexible/addrspace.h>
#include <mem/model/flexible/mapping.h>
#include <mem/process.h>

#include <memory>
#include <vector>

namespace eka2l1::mem::flexible {
    struct flexible_mem_model_chunk;

    struct flexible_mem_model_chunk_attach_info {
        flexible_mem_model_chunk *chunk_;
        std::unique_ptr<mapping> map_;
    };

    struct flexible_mem_model_process: public mem_model_process {
        std::unique_ptr<address_space> addr_space_;
        std::vector<flexible_mem_model_chunk_attach_info> attachs_;

    public:
        explicit flexible_mem_model_process(mmu_base *mmu);

        const asid address_space_id() const override;
        int create_chunk(mem_model_chunk *&chunk, const mem_model_chunk_creation_info &create_info) override;
        void delete_chunk(mem_model_chunk *chunk) override;

        void *get_pointer(const vm_address addr) override;

        bool attach_chunk(mem_model_chunk *chunk) override;
        bool detach_chunk(mem_model_chunk *chunk) override;

        void unmap_from_cpu() override;
        void remap_to_cpu() override;
    };
}