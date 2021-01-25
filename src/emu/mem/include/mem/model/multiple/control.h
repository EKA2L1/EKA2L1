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

#include <mem/model/multiple/mmu.h>
#include <mem/control.h>
#include <vector>

namespace eka2l1::mem {
    class control_multiple: public control_base {
    private:
        friend struct multiple_mem_model_process;
        friend struct multiple_mem_model_chunk;
        friend struct flexible_mem_model_process;
        friend struct flexible_mem_model_chunk;
        friend class mmu_multiple;

        std::vector<std::unique_ptr<page_directory>> dirs_;
        page_directory global_dir_;

        linear_section user_global_sec_;
        linear_section user_code_sec_;
        linear_section user_rom_sec_;
        linear_section kernel_mapping_sec_;

        std::vector<std::unique_ptr<mmu_multiple>> mmus_;

    public:
        explicit control_multiple(arm::exclusive_monitor *monitor, page_table_allocator *alloc, config::state *conf, std::size_t psize_bits = 10, const bool mem_map_old = false);
        ~control_multiple() override;

        mmu_base *get_or_create_mmu(arm::core *cc) override;

        const mem_model_type model_type() const override {
            return mem_model_type::multiple;
        }

        /**
         * \brief Get host pointer of a virtual address, in the specified address space.
         */
        void *get_host_pointer(const asid id, const vm_address addr) override;

        page_info *get_page_info(const asid id, const vm_address addr) override;

        /**
         * \brief Create or renew an address space if possible.
         * 
         * \returns An ASID identify the address space. -1 if a new one can't be create.
         */
        asid rollover_fresh_addr_space() override;
        
        /**
         * \brief Assign page tables at linear base address to page directories.
         */
        void assign_page_table(page_table *tab, const vm_address linear_addr, const std::uint32_t flags, asid *id_list = nullptr, const std::uint32_t id_list_size = 0) override;
    };
}