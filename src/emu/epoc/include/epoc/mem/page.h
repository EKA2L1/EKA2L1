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
#include <vector>

namespace eka2l1::mem {
    constexpr std::size_t PAGE_PER_TABLE_20B = 0b111111;        ///< Page per table constant for 1MB paging
    constexpr std::size_t PAGE_PER_TABLE_10B = 0b11111111;      ///< Page per table constant for 1KB paging

    constexpr std::size_t TABLE_PER_DIR_20B = 0b111111;         ///< Table per directory constant for 1MB paging
    constexpr std::size_t TABLE_PER_DIR_10B = 0b111111111111;   ///< Table per directory constant for 1KB paging

    constexpr std::uint32_t OFFSET_MASK_20B = 0b11111111111111111111;       ///< The mask to extract byte offset relative to a page, from a virtual address for 1MB paging
    constexpr std::uint32_t OFFSET_MASK_10B = 0b1111111111;                 ///< The mask to extract byte offset relative to a page, from a virtual address for 1KB paging

    constexpr std::uint32_t PAGE_TABLE_INDEX_SHIFT_20B = 26;        ///< The amount of left shifting to extract page table index in 1MB paging
    constexpr std::uint32_t PAGE_TABLE_INDEX_SHIFT_10B = 20;        ///< The amount of left shifting to extract page table index in 1KB paging

    constexpr std::uint32_t PAGE_INDEX_SHIFT_20B = 20;      ///< The amount of left shifting to extract page index in 1MB paging
    constexpr std::uint32_t PAGE_INDEX_SHIFT_10B = 10;      ///< The amount of left shifting to extract page index in 1KB paging

    constexpr std::uint32_t PAGE_INDEX_MASK_20B = 0b111111;     ///< The mask to extract page index, from a virtual address for 1MB paging
    constexpr std::uint32_t PAGE_INDEX_MASK_10B = 0b11111111;   ///< The mask to extract page index, from a virtual address for 1KB paging

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
        std::vector<page_info> pages_;
        std::uint32_t id_;
        std::size_t page_size_;
        
        std::uint32_t idx_;

    public:
        explicit page_table(const std::uint32_t id, const std::size_t page_size);

        /**
         * \brief Get a page info at the given index.
         * \param idx The index of the page info to get.
         * 
         * \returns Nullptr if index out of range, else the pointer to the page info.
         */
        page_info *get_page_info(const std::size_t idx);

        const std::uint32_t id() const {
            return id_;
        }
    };

    struct page_table_allocator {
    public:
        virtual page_table *create_new(const std::size_t psize) = 0;
        virtual page_table *get_page_table_by_id(const std::uint32_t id) = 0;
    };

    using asid = std::int32_t;

    struct page_directory {
        std::vector<page_table*> page_tabs_;
        std::size_t page_size_;

        std::uint32_t offset_mask_;
        std::uint32_t page_index_mask_;
        std::uint32_t page_index_shift_;
        std::uint32_t page_table_index_shift_;

        asid id_;
        bool occupied_;

    public:
        explicit page_directory(const std::size_t page_size, const asid id);

        void *get_pointer(const vm_address addr);
        page_info  *get_page_info(const vm_address addr);
        page_table *get_page_table(const vm_address addr);

        void set_page_table(const std::uint32_t off, page_table *tab);

        const asid id() const {
            return id_;
        }

        const bool occupied() const {
            return occupied_;
        }

        void occupied(const bool will_it) {
            occupied_ = will_it;
        }
    };
}
