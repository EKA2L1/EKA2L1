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

#include <mem/model/multiple/control.h>
#include <common/log.h>

namespace eka2l1::mem {
    control_multiple::control_multiple(arm::exclusive_monitor *monitor, page_table_allocator *alloc, config::state *conf, std::size_t psize_bits, const bool mem_map_old)
        : control_base(monitor, alloc, conf, psize_bits, mem_map_old)
        , global_dir_(page_size_bits_, 0)
        , user_global_sec_(mem_map_old ? shared_data_eka1 : shared_data, mem_map_old ? shared_data_end_eka1 : ram_drive, page_size())
        , user_code_sec_(mem_map_old ? ram_code_addr_eka1 : ram_code_addr, mem_map_old ? ram_code_addr_eka1_end : dll_static_data, page_size())
        , user_rom_sec_(mem_map_old ? rom_eka1 : rom, mem_map_old ? rom_eka1_end : global_data, page_size())
        , kernel_mapping_sec_(kernel_mapping, kernel_mapping_end, page_size()) {
    }

    control_multiple::~control_multiple() {
    }

    mmu_base *control_multiple::get_or_create_mmu(arm::core *cc) {
        for (auto &inst: mmus_) {
            if (!inst) {
                inst = std::make_unique<mmu_multiple>(reinterpret_cast<control_base*>(this),
                    cc, conf_);
                
                return inst.get();
            } else {
                if (inst->cpu_ == cc) {
                    return inst.get();
                }
            }
        }

        auto new_inst = std::make_unique<mmu_multiple>(reinterpret_cast<control_base*>(this),
            cc, conf_);

        mmus_.push_back(std::move(new_inst));

        return mmus_.back().get();
    }
    
    asid control_multiple::rollover_fresh_addr_space() {
        // Try to find existing unoccpied page directory
        for (std::size_t i = 0; i < dirs_.size(); i++) {
            if (!dirs_[i]->occupied()) {
                dirs_[i]->occupied_ = true;
                return dirs_[i]->id();
            }
        }

        // 256 is the maximum number of page directories we allow, no more.
        if (dirs_.size() == 255) {
            return -1;
        }

        // Create new one. We would start at offset 1, since 0 is for global page directory.
        dirs_.push_back(std::make_unique<page_directory>(page_size_bits_, static_cast<asid>(dirs_.size() + 1)));
        dirs_.back()->occupied_ = true;

        return static_cast<asid>(dirs_.size());
    }
    
    void control_multiple::assign_page_table(page_table *tab, const vm_address linear_addr, const std::uint32_t flags, asid *id_list, const std::uint32_t id_list_size) {
        // Extract the page directory offset
        const std::uint32_t pde_off = linear_addr >> page_table_index_shift_;
        const std::uint32_t last_off = tab ? tab->idx_ : 0;

        if (tab) {
            tab->idx_ = pde_off;
        }

        auto switch_page_table = [=](page_directory *target_dir) {
            if (tab) {
                target_dir->set_page_table(last_off, nullptr);
            }

            target_dir->set_page_table(pde_off, tab);
        };

        if (id_list != nullptr) {
            for (std::uint32_t i = 0; i < id_list_size; i++) {
                if (id_list[i] <= dirs_.size()) {
                    switch_page_table(dirs_[id_list[i] - 1].get());
                }
            }

            return;
        }

        if (flags & MMU_ASSIGN_LOCAL_GLOBAL_REGION) {
            // Iterates through all page directories and assign it
            switch_page_table(&global_dir_);

            for (auto &pde : dirs_) {
                switch_page_table(pde.get());
            }
        } else {
            if (flags & MMU_ASSIGN_GLOBAL) {
                // Assign the table to global directory
                switch_page_table(&global_dir_);
            } else {
                LOG_TRACE("Unreachable!!!");
            }
        }
    }

    void *control_multiple::get_host_pointer(const asid id, const vm_address addr) {
        if (id > 0 && dirs_.size() < id) {
            return nullptr;
        }

        if ((mem_map_old_ && (((addr >= shared_data_eka1) && (addr <= rom_eka1_end)) || addr >= ram_code_addr_eka1)) ||
            (!mem_map_old_ && (addr >= shared_data))) {
            return global_dir_.get_pointer(addr);
        }

        return ((id <= 0) ? global_dir_.get_pointer(addr) : dirs_[id - 1]->get_pointer(addr));
    }

    page_info *control_multiple::get_page_info(const asid id, const vm_address addr) {
        if (id > 0 && dirs_.size() < id) {
            return nullptr;
        }

        if ((mem_map_old_ && (((addr >= shared_data_eka1) && (addr <= rom_eka1_end)) || addr >= ram_code_addr_eka1)) ||
            (!mem_map_old_ && (addr >= shared_data))) {
            return global_dir_.get_page_info(addr);
        }

        return ((id <= 0) ? global_dir_.get_page_info(addr) : dirs_[id - 1]->get_page_info(addr));
    }
}