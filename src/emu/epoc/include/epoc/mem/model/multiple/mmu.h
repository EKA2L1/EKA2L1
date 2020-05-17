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

#include <epoc/mem/mmu.h>
#include <epoc/mem/model/section.h>

#include <memory>

namespace eka2l1::mem {
    /**
     * \brief Memory management unit for multiple model.
     */
    class mmu_multiple : public mmu_base {
        friend struct multiple_mem_model_process;
        friend struct multiple_mem_model_chunk;
        friend struct flexible_mem_model_process;
        friend struct flexible_mem_model_chunk;

        std::vector<std::unique_ptr<page_directory>> dirs_;
        page_directory *cur_dir_;
        page_directory global_dir_;

        linear_section user_global_sec_;
        linear_section user_code_sec_;
        linear_section user_rom_sec_;
        linear_section kernel_mapping_sec_;

    public:
        explicit mmu_multiple(page_table_allocator *alloc, arm::arm_interface *cpu, const std::size_t psize_bits = 10, const bool mem_map_old = false);
        ~mmu_multiple() override {}

        void *get_host_pointer(const asid id, const vm_address addr) override;

        const asid current_addr_space() const override;

        asid rollover_fresh_addr_space() override;
        bool set_current_addr_space(const asid id) override;

        page_table *get_page_table_by_addr(const vm_address addr);

        void assign_page_table(page_table *tab, const vm_address linear_addr, const std::uint32_t flags,
            asid *id_list = nullptr, const std::uint32_t id_list_size = 0) override;
    };
}
