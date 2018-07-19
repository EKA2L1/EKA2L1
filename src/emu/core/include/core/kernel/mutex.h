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

#include <core/kernel/thread.h>
#include <core/kernel/wait_obj.h>

namespace eka2l1 {
    namespace kernel {
        /*! \brief A mutex kernel object. 
        */
        class mutex : public wait_obj {
            //! The lock count
            int lock_count;

            //! Priority of this mutex.
            uint32_t priority;

            //! Thread holding
            thread_ptr holding;

        public:
            mutex(kernel_system *kern, std::string name, bool init_locked,
                kernel::access_type access = kernel::access_type::local_access);

            /*! \brief Update the priority of the mutex. */
            void update_priority();

            bool should_wait(thread_ptr thr) override;
            void acquire(thread_ptr thr) override;

            bool release(thread_ptr thr);

            void add_waiting_thread(thread_ptr thr) override;
            void erase_waiting_thread(thread_ptr thr) override;

            uint32_t get_priority() const {
                return priority;
            }
        };
    }
}