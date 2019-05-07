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
    asid mmu_multiple::rollover_fresh_addr_space() {
        // Try to find existing unoccpied page directory
        for (std::size_t i = 0; i < dirs_.size(); i++) {
            if (!dirs_[i]->occupied()) {
                return dirs_[i]->id();
            }
        }

        // 256 is the maximum number of page directories we allow, no more.
        if (dirs_.size() == 256) {
            return -1;
        }
        
        // Create new one
        dirs_.push_back(std::make_unique<page_directory>(page_size_bits_, static_cast<asid>(dirs_.size())));
        return static_cast<asid>(dirs_.size() - 1);
    }
}
