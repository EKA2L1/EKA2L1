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

#include <common/atomic.h>

#include <mem/common.h>
#include <mem/page.h>

namespace eka2l1 {
    namespace config {
        struct state;
    }

    namespace arm {
        class core;
        class exclusive_monitor;
    }
}

namespace eka2l1::mem {
    constexpr std::size_t PAGE_SIZE_BYTES_12B = 0x1000;
    constexpr std::size_t PAGE_SIZE_BYTES_20B = 0x100000;

    enum {
        MMU_ASSIGN_LOCAL_GLOBAL_REGION = 1 << 0,
        MMU_ASSIGN_GLOBAL = 1 << 1
    };

    class mmu_base;

    class control_base {
    protected:
        page_table_allocator *alloc_;
        config::state *conf_;

        arm::exclusive_monitor *exclusive_monitor_;

    public:
        std::size_t page_size_bits_; ///< The number of bits of page size.
        std::uint32_t offset_mask_;
        std::uint32_t page_index_mask_;
        std::uint32_t page_index_shift_;
        std::uint32_t page_table_index_shift_;
        std::uint32_t chunk_shift_;
        std::uint32_t chunk_size_;
        std::uint32_t chunk_mask_;
        std::uint32_t page_per_tab_shift_;

        bool mem_map_old_; ///< Should we use EKA1 mem map model?

    public:
        explicit control_base(arm::exclusive_monitor *monitor, page_table_allocator *alloc,
            config::state *conf, std::size_t psize_bits = 10, const bool mem_map_old = false);

        virtual ~control_base();

        virtual mmu_base *get_or_create_mmu(arm::core *cc) = 0;

        virtual const mem_model_type model_type() const = 0;

        /**
         * \brief Get number of bytes a page occupy
         */
        const std::size_t page_size() const {
            return page_size_bits_ == 12 ? PAGE_SIZE_BYTES_12B : PAGE_SIZE_BYTES_20B;
        }

        const bool using_old_mem_map() const {
            return mem_map_old_;
        }

        /**
         * \brief Get a page table by its ID.
         */
        page_table *get_page_table_by_id(const std::uint32_t id) {
            return alloc_->get_page_table_by_id(id);
        }

        /**
         * \brief Get host pointer of a virtual address, in the specified address space.
         */
        virtual void *get_host_pointer(const asid id, const vm_address addr) = 0;

        virtual page_info *get_page_info(const asid id, const vm_address addr) = 0;

        /**
         * @brief   Execute an exclusive write.
         * @returns -1 on invalid address, 0 on write failure, 1 on success.
         */
        template <typename T>
        std::int32_t write_exclusive(const address addr, T value, T expected,
            const mem::asid optional_asid = -1) {
            auto *real_ptr = reinterpret_cast<volatile T*>(get_host_pointer(optional_asid,
                addr));

            if (!real_ptr) {
                return -1;
            }

            return static_cast<std::int32_t>(common::atomic_compare_and_swap<T>(real_ptr, value, expected));
        }

        /**
         * \brief Create a new page table.
         * 
         * The new page table will not be assigned to any existing page directory, until
         * the user decide to assign an index to it using the MMU.
         * 
         * When we assign an index to it, it will be attached to the current page directory
         * the MMU is managing.
         */
        virtual page_table *create_new_page_table();

        /**
         * \brief Create or renew an address space if possible.
         * 
         * \returns An ASID identify the address space. -1 if a new one can't be create.
         */
        virtual asid rollover_fresh_addr_space() = 0;
        
        /**
         * \brief Assign page tables at linear base address to page directories.
         */
        virtual void assign_page_table(page_table *tab, const vm_address linear_addr, const std::uint32_t flags, asid *id_list = nullptr, const std::uint32_t id_list_size = 0) = 0;
    };
    
    using control_impl = std::unique_ptr<control_base>;

    control_impl make_new_control(arm::exclusive_monitor *monitor, page_table_allocator *alloc, config::state *conf, const std::size_t psize_bits, const bool mem_map_old,
        const mem_model_type model);
}