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

#include <cpu/12l1r/block_gen.h>
#include <common/log.h>

namespace eka2l1::arm::r12l1 {
    dashixiong_block::dashixiong_block()
        : dispatch_func_(nullptr) {
    }

    void dashixiong_block::assemble_control_funcs() {
        // TODO
    }

    translated_block *dashixiong_block::start_new_block(const vaddress addr, const asid aid) {
        bool try_new_block_result = cache_.add_block(addr, aid);
        if (!try_new_block_result) {
            LOG_ERROR(CPU_12L1R, "Trying to start a block that already exists!");
            return nullptr;
        }

        translated_block *blck = cache_.lookup_block(addr, aid);
        blck->translated_code_ = get_code_pointer();

        return blck;
    }

    bool dashixiong_block::finalize_block(translated_block *block, const std::uint32_t guest_size) {
        if (!block->translated_code_) {
            LOG_ERROR(CPU_12L1R, "This block is invalid (addr = 0x{:X})!", block->start_address());
            return false;
        }

        block->size_ = guest_size;
        block->translated_size_ = get_code_pointer() - block->translated_code_;

        return true;
    }

    translated_block *dashixiong_block::get_block(const vaddress addr, const asid aid) {
        return cache_.lookup_block(addr, aid);
    }

    void dashixiong_block::flush_range(const vaddress start, const vaddress end, const asid aid) {
        cache_.flush_range(start, end, aid);
    }

    void dashixiong_block::flush_all() {
        cache_.flush_all();
    }
}