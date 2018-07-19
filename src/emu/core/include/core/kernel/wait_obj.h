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

#include <core/kernel/kernel_obj.h>
#include <memory>
#include <vector>

namespace eka2l1 {
    namespace kernel {
        class thread;
        using thread_ptr = std::shared_ptr<thread>;

        class wait_obj : public kernel_obj {
        public:
            wait_obj(kernel_system *sys, std::string name, kernel::access_type access = access_type::local_access)
                : kernel_obj(sys, name, access) {
                waits.clear();
            }

            std::vector<thread_ptr> waits;

            virtual bool should_wait(thread_ptr thr) = 0;
            virtual void acquire(thread_ptr thr) = 0;

            virtual void add_waiting_thread(thread_ptr thr);
            virtual void erase_waiting_thread(thread_ptr thr);

            virtual void wake_up_waiting_threads();
            thread_ptr next_ready_thread();

            const std::vector<thread_ptr> &waiting_threads() const {
                return waits;
            }
        };
    }
}
