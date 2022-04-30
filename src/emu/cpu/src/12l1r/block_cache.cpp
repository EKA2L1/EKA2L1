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
    block_link::block_link()
        : linked_(false)
        , to_(0)
        , value_(nullptr) {
    }

    block_link &translated_block::get_or_add_link(const vaddress addr, const int link_pri) {
        auto res = std::find_if(links_.begin(), links_.end(), [addr](const block_link &ite) {
            return ite.to_ == addr;
        });

        if (res != links_.end()) {
            return *res;
        }

        block_link new_link;
        new_link.to_ = addr;

        if (link_pri < 0) {
            links_.push_back(new_link);
            return links_.back();
        }

        links_.insert(links_.begin() + link_pri, new_link);
        return *(links_.begin() + link_pri);
    }

    translated_block::translated_block(const vaddress start_addr)
        : hash_(start_addr)
        , size_(0)
        , last_inst_size_(0)
        , translated_code_(nullptr)
        , translated_size_(0)
        , inst_count_(0)
        , thumb_(false) {
    }

    block_cache::block_cache()
        : invalidate_callback_(nullptr) {
        // Nothing... Hee hee
    }

    bool block_cache::add_block(const vaddress start_addr) {
        // First, check if this block exists first...
        auto bl_res = blocks_.find(start_addr);
        if (bl_res != blocks_.end()) {
            return false;
        }

        std::unique_ptr<translated_block> new_block = std::make_unique<translated_block>(start_addr);
        blocks_.emplace(start_addr, std::move(new_block));

        return true;
    }

    translated_block *block_cache::lookup_block(const vaddress start_addr) {
        auto bl_res = blocks_.find(start_addr);
        if (bl_res != blocks_.end()) {
            return bl_res->second.get();
        }

        return nullptr;
    }

    void block_cache::flush_range(const vaddress range_start, const vaddress range_end) {
        // From JitBlockCache.cpp in PPSSPP!
        do {
            restart:
            auto next = blocks_.lower_bound(range_start);
            auto last = blocks_.upper_bound(range_end);

            if (next != blocks_.begin()) {
                --next;
            }

            for (; next != last; ++next) {
                const vaddress block_ite_start = next->first;
                const vaddress block_ite_end = next->second->current_address();

                if ((block_ite_start < range_end) && (block_ite_end > range_start)) {
                    if (invalidate_callback_) {
                        invalidate_callback_(next->second.get());
                    }

                    blocks_.erase(next);
                    goto restart;
                }
            }
        } while (false);
    }

    void block_cache::flush_all() {
        // Just clear all of it
        blocks_.clear();
    }
}