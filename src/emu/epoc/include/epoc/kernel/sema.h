/*
* Copyright (c) 2018 EKA2L1 Team
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

#include <common/queue.h>
#include <epoc/kernel/thread.h>

#include <memory>

namespace eka2l1 {
    class memory_system;
    using thread_ptr = std::shared_ptr<kernel::thread>;

    namespace kernel {
        class semaphore : public kernel_obj {
            int32_t avail_count;
            cp_queue<thread_ptr> waits;

            std::vector<thread_ptr> suspended;

            bool signaling;

        public:        
            semaphore(kernel_system *kern)
                : kernel_obj(kern) {
                obj_type = kernel::object_type::sema;
            }

            semaphore(kernel_system *sys, std::string sema_name,
                int32_t init_count,
                kernel::access_type access = access_type::local_access);

            void signal(int32_t signal_count);
            void wait();

            bool suspend_waiting_thread(thread *thr);
            bool unsuspend_waiting_thread(thread *thr);

            void priority_change();
        };
    }
}
