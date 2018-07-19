/*
 * Copyright (c) 2018 EKA2L1 Team / Citra Team
 * 
 * This file is part of EKA2L1 project / Citra Emulator Project.
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

#include <core/kernel/wait_obj.h>
#include <core/core_timing.h>

namespace eka2l1 {
    namespace kernel {
        enum class reset_type {
            oneshot,
            again
        };

        class timer : public wait_obj {
            timing_system *timing;

            reset_type rt;
            int callback_type;

        public:
            timer(kernel_system *kern, timing_system *timing, std::string name, reset_type rt,
                kernel::access_type access = access_type::local_access);

            ~timer();

            bool signaled;

            uint64_t init_delay;
            uint64_t interval_delay;

            bool should_wait(thread_ptr thr) override;
            void acquire(thread_ptr thr) override;

            void wake_up_waiting_threads() override;

            void set(int64_t init, int64_t interval);

            void cancel();
            void clear();

            void signal(int cycles_late);
        };
    }
}
