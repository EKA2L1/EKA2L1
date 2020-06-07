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

#include <mem/allocator/std_page_allocator.h>

namespace eka2l1::mem {
    page_table *basic_page_table_allocator::create_new(const std::size_t psize) {
        for (std::size_t i = 0; i < page_tabs_.size(); i++) {
            if (page_tabs_[i]->idx_ == static_cast<std::uint32_t>(-1)) {
                return page_tabs_[i].get();
            }
        }

        page_tabs_.push_back(std::make_unique<page_table>(static_cast<std::uint32_t>(page_tabs_.size()),
            psize));

        return page_tabs_.back().get();
    }

    page_table *basic_page_table_allocator::get_page_table_by_id(const std::uint32_t id) {
        if (page_tabs_.size() < id) {
            return nullptr;
        }

        return page_tabs_[id].get();
    }

    void basic_page_table_allocator::free_page_table(const std::uint32_t id) {
        if (page_tabs_.size() < id || !page_tabs_[id]) {
            return;
        }

        page_tabs_[id]->idx_ = static_cast<std::uint32_t>(-1);
    }
}
