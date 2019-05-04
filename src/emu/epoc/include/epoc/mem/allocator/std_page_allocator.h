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

#include <epoc/mem/page.h>

#include <atomic>
#include <memory>

namespace eka2l1::mem {
    /**
     * \brief Basic page table allocator using C++ STD's built-in functions.
     * 
     * Page table will be automatically free once the allocator is destroyed.
     */
    struct basic_page_table_allocator: public page_table_allocator {
        std::vector<std::unique_ptr<page_table>> page_tabs_;
        std::atomic<std::uint32_t> id_ct_;

    public:
        page_table *create_new() override;
        page_table *get_page_table_by_id(const std::uint32_t id) override;
    };
}
