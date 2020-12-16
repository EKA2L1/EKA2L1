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

#include <cpu/12l1r/block_cache.h>

namespace eka2l1::arm::r12l1 {
    static translated_block::hash_type make_block_hash(const vaddress start_addr, const asid aid) {
        return (static_cast<translated_block::hash_type>(aid) << 32) | start_addr;
    }

    translated_block::translated_block(const vaddress start_addr, const asid aid)
        : hash_(0)
        , size_(0)
        , translated_code_(nullptr)
        , translated_size_(0) {
        hash_ = make_block_hash(start_addr, aid);
    }

    block_cache::block_cache() {
        // Nothing... Hee hee
    }

    bool block_cache::add_block(const vaddress start_addr, const asid aid) {
        // First, check if this block exists first...
        auto bl_res = blocks_.find(make_block_hash(start_addr, aid));
        if (bl_res != blocks_.end()) {
            return false;
        }

        std::unique_ptr<translated_block> new_block = std::make_unique<translated_block>(start_addr, aid);
        blocks_.emplace(new_block->hash_, std::move(new_block));

        return true;
    }

    translated_block *block_cache::lookup_block(const vaddress start_addr, const asid aid) {
        auto bl_res = blocks_.find(make_block_hash(start_addr, aid));
        if (bl_res != blocks_.end()) {
            return bl_res->second.get();
        }

        return nullptr;
    }

    void block_cache::flush_range(const vaddress range_start, const vaddress range_end, const asid aid) {   
        auto it = blocks_.cbegin();

        while (it != blocks_.cend()) {
            auto curr_ite = it++;
            translated_block *block = curr_ite->second.get();

            if (block->address_space() == aid) {
                const vaddress orig = block->start_address();
                if ((orig >= range_start) && (orig + block->size_ <= range_end)) {
                    // Delete this block
                    blocks_.erase(curr_ite);
                }
            }
        }
    }

    void block_cache::flush_all() {
        // Just clear all of it
        blocks_.clear();
    }
}