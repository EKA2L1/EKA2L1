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

#include <common/types.h>
#include <array>

namespace eka2l1::mem {
    using vm_address = std::uint32_t;
    
    /**
     * \brief Structure contains info about a page (guest's memory chunk).
     * 
     * In real OS, this is also called a page entry.
     */
    struct page_info {
        prot perm;              ///< The permission of this page.
        void *host_addr;        ///< Pointer to the host memory chunk. Nullptr for unoccupied

        bool occupied() const {
            return host_addr;
        }
    };

    constexpr int PAGE_PER_TABLE = 1024;
    constexpr int PAGE_TABLE_PER_DIR = 1024;

    /**
     * \brief A table consists of page infos, in a specific range.
     */
    struct page_table {
        std::array<page_info, PAGE_PER_TABLE> pages_;
        std::uint32_t id_;

    public:
        explicit page_table(const std::uint32_t id)
            : id_(id) {
        }

        page_info *get_page_info(const std::size_t idx);

        const std::uint32_t id() const {
            return id_;
        }
    };

    struct page_table_allocator {
    public:
        virtual page_table *create_new() = 0;
        virtual page_table *get_page_table_by_id(const std::uint32_t id) = 0;
    };

    struct page_directory {
        std::array<page_table*, PAGE_TABLE_PER_DIR> page_tabs_;
        page_table_allocator *allocator_;

    public:
        explicit page_directory(page_table_allocator *allocator)
            : allocator_(allocator) {
        }

        page_info  *get_or_create_page_info(const vm_address addr);
        page_table *get_or_create_page_table(const vm_address addr);
    };
}
