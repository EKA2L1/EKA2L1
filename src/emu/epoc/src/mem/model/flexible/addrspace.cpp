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

#include <epoc/mem/model/flexible/addrspace.h>
#include <epoc/mem/model/flexible/mmu.h>

namespace eka2l1::mem::flexible {
    address_space::address_space(mmu_base *mmu)
        : mmu_(mmu)
        , local_data_sec_(local_data, shared_data, mmu->page_size())
        , shared_data_sec_(shared_data, ram_drive, mmu->page_size())
        , ram_code_sec_(ram_code_addr, rom_bss_addr, mmu->page_size())
        , rom_bss_sec_(rom_bss_addr, rom, mmu->page_size()) {
    }

    const asid address_space::id() const {
        return dir_->id();
    }

    linear_section *address_space::section(const std::uint32_t flags) {
        mmu_flexible *fl_mmu = reinterpret_cast<mmu_flexible*>(mmu_);

        if (flags & MEM_MODEL_CHUNK_REGION_USER_CODE) {
            return &ram_code_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_USER_GLOBAL) {
            return &shared_data_sec_;;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_USER_LOCAL) {
            return &local_data_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_ROM_BSS) {
            return &rom_bss_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_USER_ROM) {
            return &fl_mmu->rom_sec_;
        }

        if (flags & MEM_MODEL_CHUNK_REGION_KERNEL_MAPPING) {
            return &fl_mmu->kernel_mapping_sec_;
        }

        return nullptr;
    }
}