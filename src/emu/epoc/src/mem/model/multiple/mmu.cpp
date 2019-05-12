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

#include <epoc/mem/model/multiple/mmu.h>
#include <algorithm>

namespace eka2l1::mem {
    mmu_multiple::mmu_multiple(page_table_allocator *alloc, arm::arm_interface *cpu, const std::size_t psize_bits, const bool mem_map_old)
        : mmu_base(alloc, cpu, psize_bits, mem_map_old)
        , cur_dir_(nullptr)
        , global_dir_(page_size_bits_, 0)
        , user_global_sec_(global_data, ram_drive, page_size())
        , user_code_sec_(mem_map_old ? ram_code_addr_eka1 : ram_code_addr, mem_map_old ? ram_code_addr_eka1_end : rom, page_size()) {
    }
    
    asid mmu_multiple::rollover_fresh_addr_space() {
        // Try to find existing unoccpied page directory
        for (std::size_t i = 0; i < dirs_.size(); i++) {
            if (!dirs_[i]->occupied()) {
                return dirs_[i]->id();
            }
        }

        // 256 is the maximum number of page directories we allow, no more.
        if (dirs_.size() == 255) {
            return -1;
        }
        
        // Create new one. We would start at offset 1, since 0 is for global page directory.
        dirs_.push_back(std::make_unique<page_directory>(page_size_bits_, static_cast<asid>(dirs_.size() + 1)));
        return static_cast<asid>(dirs_.size());
    }

    bool mmu_multiple::set_current_addr_space(const asid id) {
        if (dirs_.size() <= id) {
            return false;
        }

        cur_dir_ = dirs_[id].get();
        return true;
    }
    
    void mmu_multiple::assign_page_table(page_table *tab, const vm_address linear_addr, const std::uint32_t flags) {
        // Extract the page directory offset
        const std::uint32_t pde_off = linear_addr >> page_table_index_shift_;
        const std::uint32_t last_off = tab->idx_;

        tab->idx_ = pde_off;

        auto switch_page_table = [=](page_directory *target_dir) {
            target_dir->set_page_table(last_off, nullptr);
            target_dir->set_page_table(pde_off, tab);
        };

        if (flags & MMU_ASSIGN_LOCAL_GLOBAL_REGION) {
            // Iterates through all page directories and assign it
            for (auto &pde: dirs_) {
                switch_page_table(pde.get());
            }
        } else {
            if (flags & MMU_ASSIGN_GLOBAL) {
                // Assign the table to global directory
                switch_page_table(&global_dir_);
            } else {
                switch_page_table(cur_dir_);
            }
        }
    }

    void *mmu_multiple::get_host_pointer(const asid id, const vm_address addr) {
        if (dirs_.size() <= id) {
            return nullptr;
        }

        return dirs_[id]->get_pointer(addr);
    }
}
