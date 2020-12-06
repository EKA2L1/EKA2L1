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

#pragma once

#include <mem/page.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace eka2l1::mem {
    class control_base;
}

namespace eka2l1::mem::flexible {
    using page_directory_instance = std::unique_ptr<page_directory>;

    /**
     * @brief Respond for managing page directory.
     */
    struct page_directory_manager {
        std::vector<page_directory_instance> dirs_;

    public:
        explicit page_directory_manager(const std::uint32_t max_dir_count);
    
        /**
         * @brief   Allocate a new page directory.
         * 
         * @param   cntr     Pointer to the Memory Control.
         * @returns Pointer to a new page directory on success.
         * 
         * @see     free
         */
        page_directory *allocate(control_base *cntr);

        /**
         * @brief   Get an existing page directory.
         * 
         * @param   id      The address space ID associated with this page directory.
         * @returns Pointer to the page directory if it's found, else nullptr.
         */
        page_directory *get(const asid id);

        /**
         * @brief   Free an existing page directory.
         * @param   id      The address space ID associated with this page directory.
         * 
         * @returns True on success
         * 
         * @see     allocate
         */
        bool free(const asid id);
    };
}