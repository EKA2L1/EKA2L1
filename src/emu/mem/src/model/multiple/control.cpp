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

#include <common/log.h>
#include <mem/model/multiple/control.h>

namespace eka2l1::mem {
    control_multiple::control_multiple(arm::exclusive_monitor *monitor, page_table_allocator *alloc, config::state *conf, std::size_t psize_bits, const bool mem_map_old)
        : control_base(monitor, alloc, conf, psize_bits, mem_map_old)
        , global_dir_(page_size_bits_, 0)
        , user_global_sec_(mem_map_old ? shared_data_eka1 : shared_data, mem_map_old ? shared_data_end_eka1 : ram_drive, page_size())
        , user_code_sec_(mem_map_old ? ram_code_addr_eka1 : ram_code_addr, mem_map_old ? ram_code_addr_eka1_end : dll_static_data, page_size())
        , user_rom_sec_(mem_map_old ? rom_eka1 : rom, mem_map_old ? kern_mapping_eka1 : global_data, page_size())
        , kernel_mapping_sec_(mem_map_old ? kern_mapping_eka1 : kernel_mapping, mem_map_old ? kern_mapping_eka1_end : kernel_mapping_end, page_size()) {
    }

    control_multiple::~control_multiple() {
    }

    mmu_base *control_multiple::get_or_create_mmu(arm::core *cc) {
        for (auto &inst : mmus_) {
            if (!inst) {
                inst = std::make_unique<mmu_multiple>(reinterpret_cast<control_base *>(this),
                    cc, conf_);

                return inst.get();
            } else {
                if (inst->cpu_ == cc) {
                    return inst.get();
                }
            }
        }

        auto new_inst = std::make_unique<mmu_multiple>(reinterpret_cast<control_base *>(this),
            cc, conf_);

        mmus_.push_back(std::move(new_inst));

        return mmus_.back().get();
    }

    asid control_multiple::rollover_fresh_addr_space() {
        // Try to find existing unoccpied page directory
        for (std::size_t i = 0; i < dirs_.size(); i++) {
            if (!dirs_[i]->occupied()) {
                dirs_[i]->occupied_ = true;
                return dirs_[i]->id();
            }
        }

        // 256 is the maximum number of page directories we allow, no more.
        if (dirs_.size() == 255) {
            return -1;
        }

        // Create new one. We would start at offset 1, since 0 is for global page directory.
        dirs_.push_back(std::make_unique<page_directory>(page_size_bits_, static_cast<asid>(dirs_.size() + 1)));
        dirs_.back()->occupied_ = true;

        return static_cast<asid>(dirs_.size());
    }

    void control_multiple::assign_page_table(page_table *tab, const vm_address linear_addr, const std::uint32_t flags, asid *id_list, const std::uint32_t id_list_size) {
        // Extract the page directory offset
        const std::uint32_t pde_off = linear_addr >> page_table_index_shift_;
        const std::uint32_t last_off = tab ? tab->idx_ : 0;

        if (tab) {
            tab->idx_ = pde_off;
        }

        auto switch_page_table = [=](page_directory *target_dir) {
            if (tab) {
                target_dir->set_page_table(last_off, nullptr);
            }

            target_dir->set_page_table(pde_off, tab);
        };

        if (id_list != nullptr) {
            for (std::uint32_t i = 0; i < id_list_size; i++) {
                if (id_list[i] <= dirs_.size()) {
                    switch_page_table(dirs_[id_list[i] - 1].get());
                }
            }

            return;
        }

        if (flags & MMU_ASSIGN_LOCAL_GLOBAL_REGION) {
            // Iterates through all page directories and assign it
            switch_page_table(&global_dir_);

            for (auto &pde : dirs_) {
                switch_page_table(pde.get());
            }
        } else {
            if (flags & MMU_ASSIGN_GLOBAL) {
                // Assign the table to global directory
                switch_page_table(&global_dir_);
            } else {
                LOG_TRACE(MEMORY, "Unreachable!!!");
            }
        }
    }

    inline bool should_addr_from_global(const vm_address addr, const bool mem_map_old) {
        bool should_from_global = false;

        if (mem_map_old) {
            should_from_global = ((addr >= shared_data_eka1) && (addr <= kern_mapping_eka1_end)) || (addr >= dll_static_data_eka1_end);
        } else {
            should_from_global = ((addr >= shared_data) && (addr < dll_static_data_flexible)) || (addr >= rom);
        }

        return should_from_global;
    }

    void *control_multiple::get_host_pointer(const asid id, const vm_address addr) {
        if ((id > 0) && (dirs_.size() < id)) {
            return nullptr;
        }

        if (should_addr_from_global(addr, mem_map_old_)) {
            return global_dir_.get_pointer(addr);
        }

        return ((id <= 0) ? global_dir_.get_pointer(addr) : dirs_[id - 1]->get_pointer(addr));
    }

    page_info *control_multiple::get_page_info(const asid id, const vm_address addr) {
        if ((id > 0) && (dirs_.size() < id)) {
            return nullptr;
        }

        if (should_addr_from_global(addr, mem_map_old_)) {
            return global_dir_.get_page_info(addr);
        }

        return ((id <= 0) ? global_dir_.get_page_info(addr) : dirs_[id - 1]->get_page_info(addr));
    }

    std::optional<std::uint32_t> control_multiple::read_dword_data_from(const asid from_id, const asid reader_id, const vm_address addr) {
        if ((addr >= page_dir_start_eka1) && (addr < page_dir_end_eka1)) {
            // Physical address of the page table at the index indicated in the address. But we don't have that kind of resource, so just return what is needed
            // Mirror the linear to physical, add attribute. 1 = page table, 2 = section
            return static_cast<std::uint32_t>(addr << 10) | 0b1;
        } else if ((addr >= page_table_info_start_eka1) && (addr < page_table_info_end_eka1)) {
            // Page table info, with the last 6 bits containg attribute, and the remaining 26 bits containing the offset of the page table in the page table
            // memory region
            if (page_size_bits_ == 20) {
                // Get the page table size in 4 bytes (the system is 32-bit, so physical address still should be in 4 bytes, multiply with the index of the page table, retrieved by subtracting from the base,
                // and divide by single info size - 4 bytes)
                return (static_cast<std::uint32_t>((((addr - page_table_info_start_eka1) >> 2) * (PAGE_PER_TABLE_20B * 4))) << 6) | 0b1;
            } else {
                // 0b10 = 2 indicates 12-bit page
                return (static_cast<std::uint32_t>(((addr - page_table_info_start_eka1) >> 2) * (PAGE_PER_TABLE_12B * 4)) << 6) | 0b10;
            }
        } else if ((addr >= page_table_start_eka1) && (addr < page_table_end_eka1)) {
            // Calculate the page table index
            const std::uint32_t page_table_size_shift = (((page_size_bits_ == 20) ? PAGE_PER_TABLE_SHIFT_20B : PAGE_PER_TABLE_SHIFT_12B) + 2);
            const std::uint32_t page_table_index = (addr - page_table_start_eka1) >> page_table_size_shift;
            const std::uint32_t page_index = (((addr - page_table_start_eka1) - (1 << page_table_size_shift) * page_table_index)) >> 2;

            // Construct a linear address that when it's assigned using write_data_to, we can assign necessary page/page table from host.
            return (page_index << ((page_size_bits_ == 20) ? PAGE_INDEX_SHIFT_20B : PAGE_INDEX_SHIFT_12B)) | (page_table_index << ((page_size_bits_ == 20) ? PAGE_TABLE_INDEX_SHIFT_20B : PAGE_TABLE_INDEX_SHIFT_12B));
        }

        std::uint32_t *host_ptr = reinterpret_cast<std::uint32_t*>(get_host_pointer(from_id, addr));
        if (!host_ptr) {
            return std::nullopt;
        }

        return *host_ptr;
    }

    bool control_multiple::write_dword_data_to(const asid to_id, const asid writer_id, const vm_address addr, const std::uint32_t data) {
        if ((dirs_.size() < writer_id) || (dirs_.size() < to_id)) {
            return false;
        }

        if ((addr >= page_dir_start_eka1) && (addr < page_dir_end_eka1)) {
            if (to_id != global_dir_.id_) {
                LOG_ERROR(MEMORY, "Permission denied on the emulator (at the moment): page table injection to non-global/kernel process! {}", to_id);
                return false;
            }

            // Assign physical page table address. The agenda we want to do here is assign page table from "writer directory" to the "to directory".
            page_directory *source_dir = dirs_[writer_id - 1].get();
            if (!source_dir) {
                LOG_WARN(MEMORY, "Trying to assign page table with a physical address, but the page directory does not exist!");
                return false;
            }

            const std::uint32_t dest_page_table_index = (addr - page_dir_start_eka1) >> 2;
            page_table *source_pt = source_dir->get_page_table(data);

            if (to_id == global_dir_.id_) {
                if (!global_dir_.set_page_table(dest_page_table_index, source_pt)) {
                    LOG_ERROR(MEMORY, "Can't set page table responsible for address 0x{:X} to page directory entry 0x{:X}", data, addr);
                    return false;
                } else {
                    return true;
                }
            } else {
                if (!dirs_[to_id - 1]->set_page_table(dest_page_table_index, source_pt)) {
                    LOG_ERROR(MEMORY, "Can't set page table responsible for address 0x{:X} to page directory entry 0x{:X}", data, addr);
                    return false;
                } else {
                    return true;
                }
            }
        } else if ((addr >= page_table_info_start_eka1) && (addr < page_table_info_end_eka1)) {
            LOG_ERROR(MEMORY, "Page table info writing is not supported on the emulator!");
            return false;
        } else if ((addr >= page_table_start_eka1) && (addr < page_table_end_eka1)) {
            LOG_ERROR(MEMORY, "Page table writing is not supported on the emulator!");
            return false;
        }

        std::uint32_t *host_ptr = reinterpret_cast<std::uint32_t*>(get_host_pointer(to_id, addr));
        if (!host_ptr) {
            return false;
        }

        *host_ptr = data;
        return true;
    }
}