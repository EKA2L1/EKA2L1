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

#include <cpu/arm_interface.h>

#include <mem/mem.h>
#include <mem/allocator/std_page_allocator.h>
#include <mem/mmu.h>
#include <mem/ptr.h>

#include <algorithm>

namespace eka2l1 {
    memory_system::memory_system(config::state *conf, const mem::mem_model_type model_type, const bool mem_map_old)
        : conf_(conf) {
        alloc_ = std::make_unique<mem::basic_page_table_allocator>();
        impl_ = mem::make_new_control(alloc_.get(), conf_, 12, mem_map_old, model_type);
    }

    memory_system::~memory_system() {
        if (rom_map_) {
            common::unmap_file(rom_map_);
        }
    }

    mem::mmu_base *memory_system::get_mmu(arm::core *cc) {
        return impl_->get_or_create_mmu(cc);
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
