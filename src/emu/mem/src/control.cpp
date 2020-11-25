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

#include <cpu/arm_interface.h>

#include <mem/control.h>
#include <mem/mmu.h>

#include <mem/model/flexible/control.h>
#include <mem/model/multiple/control.h>

namespace eka2l1::mem {
    control_base::control_base(arm::exclusive_monitor *monitor, page_table_allocator *alloc, config::state *conf,
        std::size_t psize_bits, const bool mem_map_old)
        : alloc_(alloc)
        , conf_(conf)
        , page_size_bits_(psize_bits)
        , mem_map_old_(mem_map_old)
        , exclusive_monitor_(monitor) {
        if (psize_bits == 20) {
            offset_mask_ = OFFSET_MASK_20B;
            page_table_index_shift_ = PAGE_TABLE_INDEX_SHIFT_20B;
            page_index_mask_ = PAGE_INDEX_MASK_20B;
            page_index_shift_ = PAGE_INDEX_SHIFT_20B;
            chunk_shift_ = CHUNK_SHIFT_20B;
            chunk_mask_ = CHUNK_MASK_20B;
            chunk_size_ = CHUNK_SIZE_20B;
            page_per_tab_shift_ = PAGE_PER_TABLE_SHIFT_20B;
        } else {
            offset_mask_ = OFFSET_MASK_12B;
            page_table_index_shift_ = PAGE_TABLE_INDEX_SHIFT_12B;
            page_index_mask_ = PAGE_INDEX_MASK_12B;
            page_index_shift_ = PAGE_INDEX_SHIFT_12B;
            chunk_shift_ = CHUNK_SHIFT_12B;
            chunk_mask_ = CHUNK_MASK_12B;
            chunk_size_ = CHUNK_SIZE_12B;
            page_per_tab_shift_ = PAGE_PER_TABLE_SHIFT_12B;
        }

        if (exclusive_monitor_) {
            exclusive_monitor_->read_8bit = [this](arm::core *core, const vm_address addr, std::uint8_t* data) {
                mmu_base *mm = get_or_create_mmu(core);
                return mm->read_8bit_data(addr, data);
            };
            
            exclusive_monitor_->read_16bit = [this](arm::core *core, const vm_address addr, std::uint16_t* data) {
                mmu_base *mm = get_or_create_mmu(core);
                return mm->read_16bit_data(addr, data); 
            };

            exclusive_monitor_->read_32bit = [this](arm::core *core, const vm_address addr, std::uint32_t* data) {
                mmu_base *mm = get_or_create_mmu(core);
                return mm->read_32bit_data(addr, data);
            };

            exclusive_monitor_->read_64bit = [this](arm::core *core, const vm_address addr, std::uint64_t* data) {
                mmu_base *mm = get_or_create_mmu(core);
                return mm->read_64bit_data(addr, data);
            };

            exclusive_monitor_->write_8bit = [this](arm::core *core, const vm_address addr, std::uint8_t value, std::uint8_t expected) {
                mmu_base *mm = get_or_create_mmu(core);
                return mm->write_exclusive<std::uint8_t>(addr, value, expected);
            };

            exclusive_monitor_->write_16bit = [this](arm::core *core, const vm_address addr, std::uint16_t value, std::uint16_t expected) {
                mmu_base *mm = get_or_create_mmu(core);
                return mm->write_exclusive<std::uint16_t>(addr, value, expected);
            };

            exclusive_monitor_->write_32bit = [this](arm::core *core, const vm_address addr, std::uint32_t value, std::uint32_t expected) {
                mmu_base *mm = get_or_create_mmu(core);
                return mm->write_exclusive<std::uint32_t>(addr, value, expected);
            };

            exclusive_monitor_->write_64bit = [this](arm::core *core, const vm_address addr, std::uint64_t value, std::uint64_t expected) {
                mmu_base *mm = get_or_create_mmu(core);
                return mm->write_exclusive<std::uint64_t>(addr, value, expected);
            };
        }
    }
    
    control_base::~control_base() {
    }
    
    page_table *control_base::create_new_page_table() {
        return alloc_->create_new(page_size_bits_);
    }
    
    control_impl make_new_control(arm::exclusive_monitor *monitor, page_table_allocator *alloc, config::state *conf, const std::size_t psize_bits, const bool mem_map_old,
        const mem_model_type model) {
        switch (model) {
        case mem_model_type::multiple: {
            return std::make_unique<control_multiple>(monitor, alloc, conf, psize_bits, mem_map_old);
        }

        case mem_model_type::flexible: {
            return std::make_unique<flexible::control_flexible>(monitor, alloc, conf, psize_bits, mem_map_old);
        }

        default:
            break;
        }

        return nullptr;
    }
    
}