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

#include <common/types.h>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace eka2l1::mem {
    class mmu_base;
}

namespace eka2l1::mem::flexible {
    struct mapping;

    /**
     * @brief Represents a memory range allocated from host memory.
     */
    struct memory_object {
    protected:
        void *data_;                    ///< Pointer to the virtual memory allocated from host.
        std::size_t page_occupied_;     ///< Number of pages this memory object occupied.

        mmu_base *mmu_;
        bool external_;

        std::vector<mapping*> mappings_;

    public:
        explicit memory_object(mmu_base *mmu, const std::size_t page_count, void *external_host);
        ~memory_object();

        void *ptr() {
            return data_;
        }

        std::size_t page_count() const {
            return page_occupied_;
        }

        /**
         * @brief       Attach new mapping.
         * 
         * @param       layout Pointer to the mapping.
         * @returns     True on success, if the mapping has not been attached.
         */
        bool attach_mapping(mapping *layout);

        /**
         * @brief       Detach existing mapping.
         * 
         * @param       layout Pointer to the mapping to detach.
         * @returns     True on success.
         */
        bool detach_mapping(mapping *layout);

        /**
         * @brief       Commit pages to the host.
         * 
         * The number of pages to commit must not exceed the maximum page count specified when
         * constructing this memory object.
         * 
         * Also, the region to commit must not also exceed the maximum page count.
         * 
         * @param       page_offset   Starting page offset for the commit.
         * @param       total_pages   Number of pages to commit.
         * @param       perm          The permissions for newly committed pages.
         *
         * @returns     True on success.
         */
        bool commit(const std::uint32_t page_offset, const std::size_t total_pages, const prot perm);

        /**
         * @brief       Decommit pages from the host.
         * 
         * The number of pages to decommit must not exceed the maximum page count specified when
         * constructing this memory object.
         * 
         * Also, the region to commit must not also exceed the maximum page count.
         * 
         * @param       page_offset   Starting page offset for the commit.
         * @param       total_pages   Number of pages to decommit.
         * 
         * @returns     True on success.
         */
        bool decommit(const std::uint32_t page_offset, const std::size_t total_pages);
    };
}