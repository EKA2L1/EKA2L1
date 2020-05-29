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

#include <mem/model/flexible/mapping.h>
#include <mem/model/flexible/memobj.h>
#include <mem/chunk.h>

#include <common/algorithm.h>
#include <common/allocator.h>

namespace eka2l1::mem::flexible {
    struct flexible_mem_model_process;

    struct flexible_mem_model_chunk: public mem_model_chunk {
    protected:
        friend struct flexible_mem_model_process;

        std::uint32_t max_size_;
        std::uint32_t committed_;
        std::uint32_t flags_;

        flexible_mem_model_process *owner_;

        std::unique_ptr<memory_object> mem_obj_;
        std::unique_ptr<common::bitmap_allocator> page_bma_;

        vm_address fixed_addr_;
        std::unique_ptr<mapping> fixed_mapping_;

    public:
        explicit flexible_mem_model_chunk(mmu_base *mmu, const asid id);
        ~flexible_mem_model_chunk() override;
        
        const vm_address base(mem_model_process *process) override;

        void *host_base() override;

        const std::size_t committed() const override {
            return committed_;
        }

        const std::size_t max() const override {
            return max_size_;
        }

        int do_create(const mem_model_chunk_creation_info &create_info) override;

        std::size_t commit(const vm_address offset, const std::size_t size) override;
        void decommit(const vm_address offset, const std::size_t size) override;

        bool allocate(const std::size_t size) override;

        void unmap_from_cpu(mem_model_process *pr) override;
        void map_to_cpu(mem_model_process *pr) override;
    };
}