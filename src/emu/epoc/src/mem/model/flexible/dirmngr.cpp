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

#include <epoc/mem/model/flexible/dirmngr.h>
#include <epoc/mem/mmu.h>

namespace eka2l1::mem::flexible {
    page_directory_manager::page_directory_manager(const std::uint32_t max_dir_count) {
        dirs_.resize(max_dir_count);
    }

    page_directory *page_directory_manager::allocate(mmu_base *mmu) {
        // Search for empty, or unoccupied directory (freed)
        bool suit_found = false;

        for (std::size_t i = 0; i < dirs_.size(); i++) {
            if (!dirs_[i]) {
                dirs_[i] = std::make_unique<page_directory>(mmu->page_size(), static_cast<asid>(i));
                suit_found = true;
            } else if (!dirs_[i]->occupied_) {
                dirs_[i]->occupied_ = true;
                suit_found = true;
            }

            if (suit_found) {
                return dirs_[i].get();
            }
        }
        
        // No more directory for you, told the emulator to stop being greedy.
        return nullptr;
    }

    bool page_directory_manager::free(const asid id) {
        if (id >= dirs_.size() || id < 0) {
            return false;
        }

        if (!dirs_[id] || !dirs_[id]->occupied_) {
            return false;
        }

        dirs_[id]->occupied_ = false;
        return true;
    }

    page_directory *page_directory_manager::get(const asid id) {
        for (std::size_t i = 0; i < dirs_.size(); i++) {
            if (dirs_[i] && dirs_[i]->occupied_ && (dirs_[i]->id_ == id)) {
                return dirs_[i].get();
            }
        }

        return nullptr;
    }
}