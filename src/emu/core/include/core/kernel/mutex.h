/*
 * Copyright (c) 2018 EKA2L1 Team / Citra Emulator Project
 * 
 * This file is part of EKA2L1 project / Citra Emulator Project
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

#include <kernel/wait_obj.h>
#include <kernel/thread.h>

namespace eka2l1 {
    namespace kernel {
        class mutex : public wait_obj {
            int lock_count;
            uint32_t priority;
            thread_ptr holding;

        public:
            mutex(kernel_system *kern, std::string name, bool init_locked,
                kernel::owner_type own,
                kernel::uid own_id,
                kernel::access_type access = kernel::access_type::local_access);

            void update_priority();

            bool should_wait(kernel::uid thr_id) override;
            void acquire(kernel::uid thr_id) override;

            bool release(thread_ptr thr);

            void add_waiting_thread(thread_ptr thr) override;
            void erase_waiting_thread(kernel::uid thr) override;

            uint32_t get_priority() const {
                return priority;
            }
        };
    }
}