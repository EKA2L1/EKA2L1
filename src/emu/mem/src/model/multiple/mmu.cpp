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

#include <algorithm>
#include <mem/model/multiple/mmu.h>
#include <mem/model/multiple/control.h>

namespace eka2l1::mem {
    mmu_multiple::mmu_multiple(control_base *manager, arm::core *cpu, config::state *conf)
        : mmu_base(manager, cpu, conf)
        , cur_dir_(nullptr) {
        cur_dir_ = &(reinterpret_cast<control_multiple*>(manager)->global_dir_);
    }

    bool mmu_multiple::set_current_addr_space(const asid id) {
        control_multiple *ctrl_mul = reinterpret_cast<control_multiple*>(manager_);

        if (id == 0) {
            cur_dir_ = &ctrl_mul->global_dir_;
            return true;
        }

        if (id != 0 && ctrl_mul->dirs_.size() < id) {
            return false;
        }

        cur_dir_ = ctrl_mul->dirs_[id - 1].get();
        return true;
    }

    page_table *mmu_multiple::get_page_table_by_addr(const vm_address addr) {
        return cur_dir_->get_page_table(addr);
    }
    
    const asid mmu_multiple::current_addr_space() const {
        if (cur_dir_) {
            return cur_dir_->id();
        }

        return 0;
    }

    void *mmu_multiple::get_host_pointer(const vm_address addr) {
        if (!cur_dir_) {
            return nullptr;
        }

        return cur_dir_->get_pointer(addr);
    }

    page_info *mmu_multiple::get_page_info(const vm_address addr) {
        if (!cur_dir_) {
            return nullptr;
        }

        return cur_dir_->get_page_info(addr);
    }
}
