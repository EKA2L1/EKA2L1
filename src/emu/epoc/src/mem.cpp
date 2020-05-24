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

#include <epoc/mem.h>
#include <epoc/mem/allocator/std_page_allocator.h>
#include <epoc/mem/mmu.h>
#include <epoc/ptr.h>

#include <algorithm>

namespace eka2l1 {
    void memory_system::init(arm::arm_interface *jit, const mem::mem_model_type model_type, const bool mem_map_old) {
        alloc_ = std::make_unique<mem::basic_page_table_allocator>();
        impl_ = mem::make_new_mmu(alloc_.get(), jit, 12, mem_map_old, model_type);
        cpu_ = jit;
    }

    void memory_system::shutdown() {
        if (rom_map_) {
            common::unmap_file(rom_map_);
        }
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
