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
#include <algorithm>
#include <core/kernel/thread.h>
#include <core/kernel/wait_obj.h>
#include <core/kernel/scheduler.h>

#include <core/core_kernel.h>

namespace eka2l1 {
    namespace kernel {
        // Based on Citra

        void wait_obj::add_waiting_thread(thread_ptr thr) {
            auto res = std::find_if(waits.begin(), waits.end(),
                [thr](auto &thr_ptr) { return thr_ptr == thr; });

            if (res == waits.end()) {
                waits.push_back(thr);

                // Definitely this must be changed, i dont really want this
                // TODO (bentokun): Replace this with dynamic pointer
                thr->waits_on.push_back(this);
            }
        }

        void wait_obj::erase_waiting_thread(thread_ptr thr) {
            auto res = std::find_if(waits.begin(), waits.end(),
                [thr](auto &thr_ptr) { return thr == thr_ptr; });

            if (res != waits.end()) {
                waits.erase(res);
            }
        }

        thread_ptr wait_obj::next_ready_thread() {
            thread_ptr next = nullptr;
            uint32_t next_pri = kernel::priority_absolute_background + 1;

            for (const auto &wait_thread : waits) {
                if (wait_thread->current_real_priority() >= next_pri) {
                    continue;
                }

                if (should_wait(wait_thread)) {
                    continue;
                }

                bool ready_to_run = true;

                if (wait_thread->current_state() == thread_state::wait_fast_sema) {
                    ready_to_run = std::none_of(wait_thread->waits.begin(), wait_thread->waits.end(),
                        [wait_thread](auto &obj) { return obj->should_wait(wait_thread); });
                }

                if (ready_to_run) {
                    next = wait_thread;
                    next_pri = wait_thread->current_real_priority();
                }
            }

            return next;
        }

        void wait_obj::wake_up_waiting_threads() {
            while (auto thr = next_ready_thread()) {
                 for (auto &obj : thr->waits_on) {
                    obj->acquire(thr);
                }

                for (auto &obj : thr->waits_on) {
                    obj->erase_waiting_thread(thr);
                }

                thr->waits.clear();
                thr->get_scheduler()->resume(thr);
            }
        }
    }
}
