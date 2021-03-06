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
#include <mem/common.h>
#include <vector>

namespace eka2l1::mem {
    enum : uint32_t {
        null_trap = 0x00000000,
        local_data = 0x00400000,
        dll_static_data = 0x38000000,
        shared_data = 0x40000000,
        ram_code_addr = 0x70000000,
        dll_static_data_flexible = 0x78000000,
        rom = 0x80000000,
        global_data = 0x90000000,
        ram_drive = 0xA0000000,
        mmu = 0xC0000000,
        io_mapping = 0xC3000000,
        page_tables = 0xC4000000,
        umem = 0xC8000000,
        kernel_mapping = 0xC9200000,
        kernel_mapping_end = 0xFFFF0000,
        rom_eka1 = 0x50000000,
        kern_mapping_eka1 = 0x58000000,
        kern_mapping_eka1_end = 0x60000000,
        rom_bss_eka1 = 0x80000000,
        dll_static_data_eka1 = 0x86000000,
        dll_static_data_eka1_end = 0x88000000,
        ram_code_addr_eka1 = 0xE0000000,
        ram_code_addr_eka1_end = 0xF0000000,
        shared_data_eka1 = 0x10000000,
        shared_data_end_eka1 = 0x30000000,
        shared_data_section_size_eka2 = 0x30000000,
        shared_data_section_size_eka1 = 0x20000000,
        shared_data_section_size_max = shared_data_section_size_eka2,
        code_seg_section_size = 0x10000000,
        page_size = 0x1000,
        page_bits = 12,
        page_table_number_entries = 1 << (32 - page_bits),
        shared_data_section_max_number_entries = shared_data_section_size_max / page_size,
        code_seg_section_number_entries = code_seg_section_size / page_size
    };

    constexpr std::size_t PAGE_PER_TABLE_20B = 0b111111 + 1; ///< Page per table constant for 1MB paging
    constexpr std::size_t PAGE_PER_TABLE_12B = 0b11111111 + 1; ///< Page per table constant for 4KB paging

    constexpr std::size_t TABLE_PER_DIR_20B = 0b111111 + 1; ///< Table per directory constant for 1MB paging
    constexpr std::size_t TABLE_PER_DIR_12B = 0b111111111111 + 1; ///< Table per directory constant for 4KB paging

    constexpr std::uint32_t OFFSET_MASK_20B = 0b11111111111111111111; ///< The mask to extract byte offset relative to a page, from a virtual address for 1MB paging
    constexpr std::uint32_t OFFSET_MASK_12B = 0b111111111111; ///< The mask to extract byte offset relative to a page, from a virtual address for 4KB paging

    constexpr std::uint32_t PAGE_TABLE_INDEX_SHIFT_20B = 26; ///< The amount of left shifting to extract page table index in 1MB paging
    constexpr std::uint32_t PAGE_TABLE_INDEX_SHIFT_12B = 20; ///< The amount of left shifting to extract page table index in 4KB paging

    constexpr std::uint32_t PAGE_INDEX_SHIFT_20B = 20; ///< The amount of left shifting to extract page index in 1MB paging
    constexpr std::uint32_t PAGE_INDEX_SHIFT_12B = 12; ///< The amount of left shifting to extract page index in 4KB paging

    constexpr std::uint32_t PAGE_INDEX_MASK_20B = 0b111111; ///< The mask to extract page index, from a virtual address for 1MB paging
    constexpr std::uint32_t PAGE_INDEX_MASK_12B = 0b11111111; ///< The mask to extract page index, from a virtual address for 4KB paging

    constexpr std::uint32_t CHUNK_SHIFT_12B = 20; ///< The shift of a chunk (page tables full) for 4KB paging
    constexpr std::uint32_t CHUNK_SHIFT_20B = 26; ///< The shift of a chunk (page tables full) for 1MB paging

    constexpr std::uint32_t CHUNK_SIZE_12B = 1 << 20; ///< The size of a chunk (page tables full) for 4KB paging
    constexpr std::uint32_t CHUNK_SIZE_20B = 1 << 26; ///< The size of a chunk (page tables full) for 1MB paging

    constexpr std::uint32_t CHUNK_MASK_12B = CHUNK_SIZE_12B - 1; ///< The mask of a chunk (page tables full) for 4KB paging
    constexpr std::uint32_t CHUNK_MASK_20B = CHUNK_SIZE_20B - 1; ///< The mask of a chunk (page tables full) for 1MB paging

    constexpr std::uint32_t PAGE_PER_TABLE_SHIFT_12B = 8;
    constexpr std::uint32_t PAGE_PER_TABLE_SHIFT_20B = 8;

    constexpr const std::uint32_t ROM_BSS_START_OFFSET = 0x6000000;
    constexpr const std::uint32_t ROM_BSS_START_OFFSET_EKA1 = 0;
    constexpr const std::size_t MAX_ROM_BSS_SECT_SIZE = 0x2000000;

    /**
     * \brief Structure contains info about a page (guest's memory chunk).
     * 
     * In real OS, this is also called a page entry.
     */
    struct page_info {
        prot perm; ///< The permission of this page.
        void *host_addr; ///< Pointer to the host memory chunk. Nullptr for unoccupied

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

        std::uint32_t idx_{ 0xFFFFFFFF };

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

        const std::uint32_t count() const {
            return static_cast<std::uint32_t>(pages_.size());
        }
    };

    struct page_table_allocator {
    public:
        virtual ~page_table_allocator() {}

        virtual page_table *create_new(const std::size_t psize) = 0;
        virtual page_table *get_page_table_by_id(const std::uint32_t id) = 0;

        virtual void free_page_table(const std::uint32_t id) = 0;
    };

    struct page_directory {
        std::vector<page_table *> page_tabs_;
        std::size_t page_size_;

        std::uint32_t offset_mask_;
        std::uint32_t page_index_mask_;
        std::uint32_t page_index_shift_;
        std::uint32_t page_table_index_shift_;

        asid id_;
        bool occupied_{ false };

    public:
        explicit page_directory(const std::size_t page_size, const asid id);

        void *get_pointer(const vm_address addr);
        page_info *get_page_info(const vm_address addr);
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
