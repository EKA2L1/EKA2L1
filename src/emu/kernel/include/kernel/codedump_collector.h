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

#pragma once

#include <kernel/codeseg.h>
#include <common/linked.h>

namespace eka2l1 {
    class ntimer;
}

namespace eka2l1::kernel {

    /**
     * @brief A class that collects attached code that is supposed to detach from a process.
     * 
     * The collector stores and prevent them from being unloaded immediately, only after the amount of unloaded segment
     * reached an threshold, the collector will request the code segment to unload the code.
     *
     * If the code is requested to be load again to the process, it will be removed from the collector.
     */
    class codedump_collector {
    private:
        common::roundabout attach_list_;
        common::roundabout in_decease_list_;

        std::int64_t collected_memory_ = 0;
        std::uint32_t in_list_count_ = 0;

        void clean_impl();
        void cleanup(std::uint64_t upcoming_memory_size);

    public:
        explicit codedump_collector();

        void add(kernel::codeseg::attached_info &attach_info);
        void remove( kernel::codeseg::attached_info &attach_info);

        void add(kernel::codeseg *seg);
        void remove(kernel::codeseg *seg);

        void force_clean() {
            clean_impl();
        }
    };
}