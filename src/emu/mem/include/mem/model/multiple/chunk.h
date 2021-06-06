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

#include <common/allocator.h>
#include <mem/chunk.h>
#include <mem/model/section.h>

#include <memory>
#include <vector>

namespace eka2l1::mem {
    struct mem_model_process;

    struct multiple_mem_model_chunk : public mem_model_chunk {
        vm_address base_;
        void *host_base_;

        mem_model_process *own_process_{ nullptr };

        std::size_t chunk_id_in_mmp_;

        std::size_t committed_;
        std::size_t max_size_;

        std::vector<std::uint32_t> page_tabs_;
        std::vector<asid> attached_asids_;

        std::uint16_t granularity_shift_;
        std::uint32_t create_flags_{ 0 };

        std::unique_ptr<common::bitmap_allocator> page_bma_;
        linear_section *get_section(const std::uint32_t flags);

        void do_selection_cpu_memory_manipulation(mmu_base *mmu, const bool unmap);

    public:
        bool is_local{ false };
        bool is_code { false };
        bool is_external_host{ false };

        explicit multiple_mem_model_chunk(control_base *control, const asid id)
            : mem_model_chunk(control, id) {
        }

        ~multiple_mem_model_chunk() override;

        const vm_address base(mem_model_process *process) override {
            return base_;
        }

        void *host_base() override {
            return host_base_;
        }

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

        void unmap_from_cpu(mem_model_process *pr, mmu_base *mmu) override;
        void map_to_cpu(mem_model_process *pr, mmu_base *mmu) override;
    };
}
