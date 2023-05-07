/*
 * Copyright (c) 2023 EKA2L1 Team.
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

#include <kernel/codedump_collector.h>
#include <kernel/timing.h>
#include <kernel/chunk.h>

#include <common/algorithm.h>

namespace eka2l1::kernel {
    codedump_collector::codedump_collector() {
        
    }

    void codedump_collector::clean_impl() {
        while (!attach_list_.empty()) {
            kernel::codeseg::attached_info *info = E_LOFF(attach_list_.first()->deque(), kernel::codeseg::attached_info, garbage_link);
            info->parent_seg->dump_collected(info);
        }

        if (!in_decease_list_.empty()) {
            kernel::codeseg *csg = E_LOFF(in_decease_list_.first()->deque(), kernel::codeseg, garbage_link);
            csg->decrease_access_count();
        }

        collected_memory_ = 0;
        in_list_count_ = 0;
    }

    void codedump_collector::add(kernel::codeseg::attached_info &attach_info) {
        if (!attach_info.garbage_link.alone()) {
            return;
        }

        std::uint64_t upcoming_size = attach_info.parent_seg->get_used_memory_size();
        cleanup(upcoming_size);

        attach_list_.push(&attach_info.garbage_link);

        collected_memory_ += upcoming_size;
        in_list_count_++;
    }

    void codedump_collector::remove(kernel::codeseg::attached_info &attach_info) {
        if (attach_info.garbage_link.alone()) {
            return;
        }

        attach_info.garbage_link.deque();

        collected_memory_ -= attach_info.parent_seg->get_used_memory_size();
        in_list_count_--;
    }

    void codedump_collector::add(kernel::codeseg *seg) {
        if (!seg->garbage_link.alone()) {
            return;
        }

        std::uint64_t upcoming_size = seg->get_used_memory_size();
        cleanup(upcoming_size);

        in_decease_list_.push(&seg->garbage_link);
        seg->increase_access_count();

        collected_memory_ += upcoming_size;
        in_list_count_++;
    }

    void codedump_collector::remove(kernel::codeseg *seg) {
        if (seg->garbage_link.alone()) {
            return;
        }

        seg->garbage_link.deque();

        collected_memory_ -= seg->get_used_memory_size();
        in_list_count_--;
    }

    void codedump_collector::cleanup(std::uint64_t upcoming_memory_size) {
        static constexpr std::uint32_t MAX_COLLECTED_SEGMENT = 1024;
        static constexpr std::uint64_t MAX_COLLECTED_MEMORY = common::MB(8);

        if ((collected_memory_ + upcoming_memory_size >= MAX_COLLECTED_MEMORY) || (in_list_count_ + 1 >= MAX_COLLECTED_SEGMENT)) {
            clean_impl();
        }
    }
}