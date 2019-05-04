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

#include <epoc/mem/allocator/std_page_allocator.h>

namespace eka2l1::mem {
    page_table *basic_page_table_allocator::create_new() {
        page_tabs_.push_back(std::make_unique<page_table>(++id_ct_));
        return page_tabs_.back().get();
    }

    page_table *basic_page_table_allocator::get_page_table_by_id(const std::uint32_t id) {
        auto result = std::lower_bound(page_tabs_.begin(), page_tabs_.end(), id,
            [](const std::unique_ptr<page_table> &tab_, const std::uint32_t &id_) {
                return tab_->id() < id_;
            });

        if (result == page_tabs_.end()) {
            return nullptr;
        }

        return (*result).get();
    }
}
