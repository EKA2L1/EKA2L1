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

#include <epoc/mem/page.h>

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

    page_table::page_table(const std::uint32_t id, const std::size_t page_size)
        : id_(id)
        , page_size_(page_size) {
        // Resize the table.
        if (page_size == 20) {
            pages_.resize(PAGE_PER_TABLE_20B);
        } else {
            pages_.resize(PAGE_PER_TABLE_10B);
        }
    }

    page_info *page_table::get_page_info(const std::size_t idx) {
        if (idx >= pages_.size()) {
            return nullptr;
        }

        return &pages_.at(idx);
    }

    page_directory::page_directory(const std::size_t page_size) 
        : page_size_(page_size) {
        if (page_size == 20) {
            offset_mask_ = OFFSET_MASK_20B;
            page_table_index_shift_ = PAGE_TABLE_INDEX_SHIFT_20B;
            page_index_mask_ = PAGE_INDEX_MASK_20B;
            page_index_shift_ = PAGE_INDEX_SHIFT_20B;

            page_tabs_.resize(TABLE_PER_DIR_20B);
        } else {
            offset_mask_ = OFFSET_MASK_10B;
            page_table_index_shift_ = PAGE_TABLE_INDEX_SHIFT_10B;
            page_index_mask_ = PAGE_INDEX_MASK_10B;
            page_index_shift_ = PAGE_INDEX_SHIFT_10B;
            
            page_tabs_.resize(TABLE_PER_DIR_10B);
        }
    }

    void *page_directory::get_pointer(const vm_address addr) {
        return reinterpret_cast<std::uint8_t*>(page_tabs_[addr >> page_table_index_shift_]
            ->get_page_info((addr >> page_index_shift_) & page_index_mask_)
            ->host_addr) + (addr & offset_mask_);
    }

    page_info  *page_directory::get_page_info(const vm_address addr) {
        return page_tabs_[addr >> page_table_index_shift_]->get_page_info((addr >> page_index_shift_) & page_index_mask_);
    }

    page_table *page_directory::get_page_table(const vm_address addr) {
        return page_tabs_[addr >> page_table_index_shift_];
    }
}
