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

#include <epoc/mem/page.h>
#include <epoc/mem/process.h>

#include <epoc/mem/model/multiple/chunk.h>
#include <epoc/mem/model/section.h>

namespace eka2l1::mem {
    struct multiple_mem_model_process : public mem_model_process {
    private:
        friend struct multiple_mem_model_chunk;

        asid addr_space_id_;
        linear_section user_local_sec_;

        std::vector<std::unique_ptr<multiple_mem_model_chunk>> chunks_;
        std::vector<multiple_mem_model_chunk *> attached_;

        multiple_mem_model_chunk *allocate_chunk_struct_ptr();

    public:
        explicit multiple_mem_model_process(mmu_base *mmu);

        ~multiple_mem_model_process() override {
        }

        const asid address_space_id() const override {
            return addr_space_id_;
        }

        int create_chunk(mem_model_chunk *&chunk, const mem_model_chunk_creation_info &create_info) override;
        void delete_chunk(mem_model_chunk *chunk) override;

        void *get_pointer(const vm_address addr) override;

        bool attach_chunk(mem_model_chunk *chunk) override;
        bool detach_chunk(mem_model_chunk *chunk) override;

        void unmap_locals_from_cpu() override;
        void remap_locals_to_cpu() override;
    };
};
