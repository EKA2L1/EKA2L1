/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <common/algorithm.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/virtualmem.h>

#include <arm/arm_interface.h>

#include <epoc/mem/mmu.h>
#include <epoc/mem/allocator/std_page_allocator.h>
#include <epoc/mem.h>
#include <epoc/ptr.h>

#include <algorithm>

namespace eka2l1 {
    void memory_system::init(arm::arm_interface *jit, const bool mem_map_old) {
        alloc_ = std::make_unique<mem::basic_page_table_allocator>();
        cpu_ = jit;

        // TODO: Make this modifiable
        impl_ = mem::make_new_mmu(alloc_.get(), jit, 12, mem_map_old, mem::mem_model_type::multiple);
    }

    void memory_system::shutdown() {
        if (rom_map_) {
            common::unmap_file(rom_map_);
        }
    }

    bool memory_system::map_rom(mem::vm_address addr, const std::string &path) {
        rom_addr_ = addr;
        rom_map_ = common::map_file(path, prot::read_write, 0, true);
        rom_size_ = common::file_size(path);

        LOG_TRACE("Rom mapped to address: 0x{:x}", reinterpret_cast<std::uint64_t>(rom_map_));

        // Calculate total page table
        std::uint32_t pte_count = static_cast<std::uint32_t>(rom_size_ + impl_->chunk_mask_) >> impl_->chunk_shift_;
        std::uint32_t pe_count = static_cast<std::uint32_t>((rom_size_ + impl_->page_size() - 1)) 
            >> impl_->page_size_bits_;

        // Create and assign page tables
        for (std::uint32_t i = 0; i < pte_count; i++) {
            mem::page_table *pt = impl_->create_new_page_table();
            std::uint32_t page_fill = common::min(pt->count(), pe_count);
            mem::vm_address pt_base_addr = addr + (i << impl_->chunk_shift_);
            std::uint8_t *pt_base_addr_host = reinterpret_cast<std::uint8_t*>(rom_map_)
                + (i << impl_->chunk_shift_);

            for (std::uint32_t j = 0; j < page_fill; j++) {
                pt->pages_[j].host_addr = pt_base_addr_host + (j << impl_->page_size_bits_);
                pt->pages_[j].perm = prot::read_write_exec;
            }

            impl_->assign_page_table(pt, pt_base_addr, mem::MMU_ASSIGN_GLOBAL);

            pe_count -= page_fill;
        }

        // Map to CPU
        cpu_->map_backing_mem(addr, rom_size_, reinterpret_cast<std::uint8_t*>(rom_map_), prot::read_write_exec);

        return true;
    }
    
    void *memory_system::get_real_pointer(const address addr, const mem::asid optional_asid) {
        if (addr == 0) {
            return nullptr;
        }
        
        return impl_->get_host_pointer(optional_asid, addr);
    }

    bool memory_system::read(const address addr, void *data, uint32_t size) {
        void *ptr = get_real_pointer(addr);

        if (!ptr) {
            return false;
        }

        std::memcpy(data, ptr, size);
        return true;
    }

    bool memory_system::write(const address addr, void *data, uint32_t size) {
        void *ptr = get_real_pointer(addr);

        if (!ptr) {
            return false;
        }

        std::memcpy(ptr, data, size);
        return true;
    }

    const int memory_system::get_page_size() const {
        return static_cast<int>(impl_->page_size());
    }
}
