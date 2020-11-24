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

#include <mem/model/flexible/mmu.h>
#include <mem/model/flexible/control.h>

#include <common/log.h>

namespace eka2l1::mem::flexible {
    mmu_flexible::mmu_flexible(control_base *manager, arm::core *cpu, config::state *conf)
        : mmu_base(manager, cpu, conf) {
        // Set kernel directory as the first one active
        control_flexible *ctrl_fx = reinterpret_cast<control_flexible*>(manager_);
        set_current_addr_space(ctrl_fx->kern_addr_space_->id());
    }
    
    const asid mmu_flexible::current_addr_space() const {
        return cur_dir_->id();
    }

    bool mmu_flexible::set_current_addr_space(const asid id) {
        // Try to get the page directory associated with this ID
        // Cố tìm page directory găn với cái ID này
        control_flexible *ctrl_fx = reinterpret_cast<control_flexible*>(manager_);
        page_directory *associated_dir = ctrl_fx->dir_mngr_->get(id);

        if (!associated_dir) {
            return false;
        }

        cur_dir_ = associated_dir;
        return true;
    }
}