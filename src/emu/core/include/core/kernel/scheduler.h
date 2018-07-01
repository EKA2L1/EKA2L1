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

#include <arm/jit_factory.h>
#include <common/queue.h>

#include <map>
#include <mutex>
#include <queue>
#include <vector>

namespace eka2l1 {
    class timing_system;
    class kernel_system;

    namespace kernel {
        class thread;
    }

    using thread_ptr = std::shared_ptr<kernel::thread>;

    namespace kernel {
        enum class thread_state;

        using uid = uint64_t;

        class thread_scheduler {
            std::map<uid, thread_ptr> waiting_threads;
            eka2l1::cp_queue<thread_ptr> ready_threads;
            std::map<uid, thread_ptr> running_threads;

            thread_ptr crr_thread;
            uint32_t ticks_yield;

            arm::jit_interface *jitter;

            int wakeup_evt;
            int yield_evt;

            std::mutex mut;
            timing_system *timing;
            kernel_system *kern;

        protected:
            thread_ptr next_ready_thread();

            void switch_context(thread_ptr oldt, thread_ptr newt);

        public:
            // The constructor also register all the needed event
            thread_scheduler(kernel_system *kern, timing_system *sys, arm::jit_interface &jitter);

            void reschedule();
            void unschedule_wakeup();

            bool schedule(thread_ptr thread);
            bool sleep(kernel::uid thread, uint32_t sl_time);
            bool wait_sema(kernel::uid thread);

            bool resume(kernel::uid id);
            void unschedule(kernel::uid id);

            void refresh();

            bool should_terminate(){
                return waiting_threads.empty() && running_threads.empty() 
                    && ready_threads.empty(); 
            }

            thread_ptr current_thread() const {
                return crr_thread;
            }
        };
    }
}
