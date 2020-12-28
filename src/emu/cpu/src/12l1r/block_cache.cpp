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
    translated_block::hash_type make_block_hash(const vaddress start_addr, const asid aid) {
        return (static_cast<translated_block::hash_type>(aid) << 32) | start_addr;
    }

    translated_block::translated_block(const vaddress start_addr, const asid aid)
        : hash_(0)
        , size_(0)
        , link_type_(TRANSLATED_BLOCK_LINK_AMBIGUOUS)
        , link_to_(0)
        , translated_code_(nullptr)
        , link_value_(nullptr)
        , translated_size_(0)
        , inst_count_(0) {
        hash_ = make_block_hash(start_addr, aid);
    }

    block_cache::block_cache()
        : invalidate_callback_(nullptr) {
        // Nothing... Hee hee
    }

    bool block_cache::add_block(const vaddress start_addr, const asid aid) {
        // First, check if this block exists first...
        auto bl_res = blocks_.find(std::make_pair(start_addr, aid));
        if (bl_res != blocks_.end()) {
            return false;
        }

        std::unique_ptr<translated_block> new_block = std::make_unique<translated_block>(start_addr, aid);
        blocks_.emplace(std::make_pair(start_addr, aid), std::move(new_block));

        return true;
    }

    translated_block *block_cache::lookup_block(const vaddress start_addr, const asid aid) {
        auto bl_res = blocks_.find(std::make_pair(start_addr, aid));
        if (bl_res != blocks_.end()) {
            return bl_res->second.get();
        }

        return nullptr;
    }

    void block_cache::flush_range(const vaddress range_start, const vaddress range_end, const asid aid) {
        // Borrow PPSSPP algorithm. Thank you :D
        bool all_done = false;

        do {
            auto next = blocks_.lower_bound(std::make_pair(range_start, aid));
            auto last = blocks_.upper_bound(std::make_pair(range_end, aid));

            bool any_flush = false;

            for (; next != last; ++next) {
                std::unique_ptr<translated_block> &next_block = next->second;

                if ((next_block->address_space() == aid) && (next_block->start_address() >= range_start)
                    && (next_block->start_address() <= range_end)) {
                    if (invalidate_callback_) {
                        invalidate_callback_(next_block.get());
                    }

                    blocks_.erase(next);

                    // Our iterator is now invalid.  Break and search again.
                    // Most of the time there shouldn't be a bunch of matching blocks.
                    any_flush = true;
                    break;
                }
            }

            if (!any_flush)
                all_done = true;
        } while (!all_done);
    }

    void block_cache::flush_all() {
        // Just clear all of it
        blocks_.clear();
    }
}