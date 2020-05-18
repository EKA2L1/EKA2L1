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

#include <epoc/mem/model/flexible/mmu.h>
#include <common/log.h>

namespace eka2l1::mem::flexible {
    static constexpr std::uint32_t MAX_PAGE_DIR_ALLOW = 512;

    mmu_flexible::mmu_flexible(page_table_allocator *alloc, arm::arm_interface *cpu, const std::size_t psize_bits,
        const bool mem_map_old)
        : mmu_base(alloc, cpu, psize_bits, mem_map_old)
        , kernel_dir_(nullptr)
        , rom_sec_(mem_map_old ? rom_eka1 : rom, mem_map_old ? rom_eka1_end : global_data, page_size())
        , code_sec_(ram_code_addr, rom_bss_addr, page_size())
        , kernel_mapping_sec_(kernel_mapping, kernel_mapping_end, page_size()) {
        // Instantiate the page directory manager
        dir_mngr_ = std::make_unique<page_directory_manager>(MAX_PAGE_DIR_ALLOW);
        chunk_mngr_ = std::make_unique<chunk_manager>();

        // Allocate the first page directory for kernel
        kernel_dir_ = dir_mngr_->allocate(this);
    }
    
    void *mmu_flexible::get_host_pointer(const asid id, const vm_address addr) {
        if (id == 0) {
            // Directory của kernel
            return kernel_dir_->get_pointer(addr);
        }

        // Tìm page directory trong quản lý - Find page directory in manager
        page_directory *target_dir = dir_mngr_->get(id);

        // Nếu null thì toang, rút
        if (!target_dir) {
            return nullptr;
        }

        return target_dir->get_pointer(addr);
    }
    
    const asid mmu_flexible::current_addr_space() const {
        return cur_dir_->id();
    }

    asid mmu_flexible::rollover_fresh_addr_space() {
        page_directory *new_dir = dir_mngr_->allocate(this);

        if (!new_dir) {
            // Trả về 1 ID không hợp lệ để báo là toang thật
            // Return a fake ID to report failure
            return -1;
        }

        return new_dir->id();
    }
    
    bool mmu_flexible::set_current_addr_space(const asid id) {
        // Try to get the page directory associated with this ID
        // Cố tìm page directory găn với cái ID này
        page_directory *associated_dir = dir_mngr_->get(id);

        if (!associated_dir) {
            return false;
        }

        cur_dir_ = associated_dir;
        return true;
    }

    void mmu_flexible::assign_page_table(page_table *tab, const vm_address linear_addr, const std::uint32_t flags,
        asid *id_list, const std::uint32_t id_list_size) {
        LOG_WARN("Assign page table not supported in flexible memory model!");
    }
}