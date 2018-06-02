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

#pragma once

#include <core_timing.h>
#include <kernel/wait_obj.h>

namespace eka2l1 {
    namespace kernel {
        enum class reset_type {
            oneshot,
            again
        };

        class timer : public wait_obj {
            std::string tname;
            timing_system *timing;

            reset_type rt;
            int callback_type;

        public:
            timer(timing_system *timing, std::string name, reset_type rt);
            ~timer();

            bool signaled;

            uint64_t init_delay;
            uint64_t interval_delay;

            bool should_wait(const kernel::uid thr_id) override;
            bool acquire(const kernel::uid thr_id) override;

            void wake_up_waiting_threads() override;

            void set(int64_t init, int64_t interval);

            void cancel();
            void clear();

            void signal(int cycles_late);
        };
    }
}
