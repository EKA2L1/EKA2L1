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

#include <mem/model/flexible/control.h>

namespace eka2l1::mem::flexible {
    static constexpr std::uint32_t MAX_PAGE_DIR_ALLOW = 512;

    control_flexible::control_flexible(arm::exclusive_monitor *monitor, page_table_allocator *alloc, config::state *conf, std::size_t psize_bits, const bool mem_map_old)
        : control_base(monitor, alloc, conf, psize_bits, mem_map_old)
        , rom_sec_(rom, global_data, page_size())
        , code_sec_(ram_code_addr, dll_static_data, page_size())
        , kernel_mapping_sec_(kernel_mapping, kernel_mapping_end, page_size())  {
        // Instantiate the page directory manager
        dir_mngr_ = std::make_unique<page_directory_manager>(MAX_PAGE_DIR_ALLOW);
        chunk_mngr_ = std::make_unique<chunk_manager>();

        // Allocate the first address space for kernel
        kern_addr_space_ = std::make_unique<address_space>(this);
    }

    control_flexible::~control_flexible() {
    }
    

    mmu_base *control_flexible::get_or_create_mmu(arm::core *cc) {
        for (auto &inst: mmus_) {
            if (!inst) {
                inst = std::make_unique<mmu_flexible>(reinterpret_cast<control_base*>(this),
                    cc, conf_);
                
                return inst.get();
            } else {
                if (inst->cpu_ == cc) {
                    return inst.get();
                }
            }
        }

        auto new_inst = std::make_unique<mmu_flexible>(reinterpret_cast<control_base*>(this),
            cc, conf_);

        mmus_.push_back(std::move(new_inst));

        return mmus_.back().get();
    }

    void *control_flexible::get_host_pointer(const asid id, const vm_address addr) {
        if ((id <= 0) || (addr >= (mem_map_old_ ? rom_eka1 : rom))) {
            // Directory của kernel
            return kern_addr_space_->dir_->get_pointer(addr);
        }

        //if (id == -1) {
        //   return cur_dir_->get_pointer(addr);
        //}

        // Tìm page directory trong quản lý - Find page directory in manager
        page_directory *target_dir = dir_mngr_->get(id);

        // Nếu null thì toang, rút
        if (!target_dir) {
            return nullptr;
        }

        return target_dir->get_pointer(addr);
    }
    
    page_info *control_flexible::get_page_info(const asid id, const vm_address addr) {
        if ((id <= 0) || (addr >= (mem_map_old_ ? rom_eka1 : rom))) {
            // Directory của kernel
            return kern_addr_space_->dir_->get_page_info(addr);
        }

        //if (id == -1) {
        //   return cur_dir_->get_pointer(addr);
        //}

        // Tìm page directory trong quản lý - Find page directory in manager
        page_directory *target_dir = dir_mngr_->get(id);

        // Nếu null thì toang, rút
        if (!target_dir) {
            return nullptr;
        }

        return target_dir->get_page_info(addr);
    }

    asid control_flexible::rollover_fresh_addr_space() {
        page_directory *new_dir = dir_mngr_->allocate(this);

        if (!new_dir) {
            // Trả về 1 ID không hợp lệ để báo là toang thật
            // Return a fake ID to report failure
            return -1;
        }

        return new_dir->id();
    }

    void control_flexible::assign_page_table(page_table *tab, const vm_address linear_addr, const std::uint32_t flags,
        asid *id_list, const std::uint32_t id_list_size) {
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
                page_directory *dir = dir_mngr_->get(id_list[i]);
                if (dir) {
                    switch_page_table(dir);
                }
            }

            return;
        }

        if (flags & MMU_ASSIGN_LOCAL_GLOBAL_REGION) {
            // Iterates through all page directories and assign it
            switch_page_table(kern_addr_space_->dir_);

            for (auto &pde : dir_mngr_->dirs_) {
                switch_page_table(pde.get());
            }
        } else {
            if (flags & MMU_ASSIGN_GLOBAL) {
                // Assign the table to global directory
                switch_page_table(kern_addr_space_->dir_);
            } else {
                //switch_page_table(cur_dir_);
            }
        }
    }
}