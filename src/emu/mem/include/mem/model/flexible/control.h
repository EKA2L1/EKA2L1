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

#include <mem/model/flexible/addrspace.h>
#include <mem/model/flexible/chnkmngr.h>
#include <mem/model/flexible/dirmngr.h>
#include <mem/model/flexible/mmu.h>
#include <mem/model/section.h>
#include <mem/control.h>

#include <memory>

namespace eka2l1::mem::flexible {
    struct control_flexible: public control_base {
    private:
        friend struct mmu_flexible;
        friend struct memory_object;
        friend struct address_space;
        friend struct flexible_mem_model_chunk;
        friend struct flexible_mem_model_process;

        std::unique_ptr<page_directory_manager> dir_mngr_;
        std::unique_ptr<chunk_manager> chunk_mngr_;
        std::unique_ptr<address_space> kern_addr_space_;

        linear_section  rom_sec_;                       ///< ROM linear section.
        linear_section  kernel_mapping_sec_;            ///< Kernel mapping linear section.
        linear_section  code_sec_;                      ///< Code section.

        std::vector<std::unique_ptr<mmu_flexible>> mmus_;

    public:
        explicit control_flexible(arm::exclusive_monitor *monitor, page_table_allocator *alloc, config::state *conf, std::size_t psize_bits = 10, const bool mem_map_old = false);
        ~control_flexible() override;

        mmu_base *get_or_create_mmu(arm::core *cc) override;

        const mem_model_type model_type() const override {
            return mem_model_type::flexible;
        }

        /**
         * \brief Get host pointer of a virtual address, in the specified address space.
         */
        void *get_host_pointer(const asid id, const vm_address addr) override;

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