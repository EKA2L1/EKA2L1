/*
* Copyright (c) 2018 EKA2L1 Team / Citra Team
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

#include <cassert>

#include <core/core_kernel.h>
#include <core/kernel/sema.h>

#include <common/log.h>

namespace eka2l1 {
    namespace kernel {
        semaphore::semaphore(kernel_system *sys, std::string sema_name,
            int32_t init_count,
            kernel::access_type access)
            : kernel_obj(sys, std::move(sema_name), access)
            , avail_count(init_count)
            , signaling(false) {
            obj_type = object_type::sema;
        }

        void semaphore::signal(int32_t signal_count) {
            int32_t prev_count = avail_count;
            signaling = true;

            for (size_t i = 0; i < signal_count; i++) {
                if (++avail_count <= 0) {
                    if (waits.size() != 0) {
                        thread_ptr ready_thread = std::move(waits.top());
                        assert(ready_thread->wait_obj == this);

                        ready_thread->get_scheduler()->resume(ready_thread);

                        waits.pop();

                        ready_thread->wait_obj = nullptr;
                    }
                }
            }
            
            signaling = false;
        }

        void semaphore::wait() {
            thread_ptr calling_thr = kern->crr_thread();
            --avail_count;

            if (avail_count < 0) {
                assert(!calling_thr->wait_obj);

                waits.push(calling_thr);
                calling_thr->get_scheduler()->wait(calling_thr);

                calling_thr->state = thread_state::wait_fast_sema;
                calling_thr->wait_obj = this;
            }
        }

        void semaphore::priority_change() {
            waits.resort();
        }

        bool semaphore::suspend_waiting_thread(thread *thr) {
            // Putting this thread into another suspend state
            if (thr->current_state() != thread_state::wait_fast_sema) {
                LOG_ERROR("Calling semaphore to suspend a thread that's not waiting");
                return false;
            }

            const auto thr_ite = std::find_if(waits.begin(), waits.end(),
                [thr](const thread_ptr &wait_thr) { return wait_thr.get() == thr; });

            if (thr_ite == waits.end()) {
                LOG_ERROR("Thread given is not found in waits");
                return false;
            }

            thread_ptr thr_sptr = *thr_ite; // Make a copy of this
            waits.remove(thr_sptr);
            suspended.push_back(thr_sptr);

            thr_sptr->state = thread_state::wait_fast_sema_suspend;

            return true;
        }

        bool semaphore::unsuspend_waiting_thread(thread *thr) {
            // Putting this thread into another suspend state
            if (thr->current_state() != thread_state::wait_fast_sema_suspend) {
                LOG_ERROR("Calling semaphore to unsuspended a thread that's not suspended");
                return false;
            }

            const auto thr_ite = std::find_if(suspended.begin(), suspended.end(),
                [thr](const thread_ptr &sus_thr) { return sus_thr.get() == thr; });

            if (thr_ite == waits.end()) {
                LOG_ERROR("Thread given is not found in suspended");
                return false;
            }

            thread_ptr thr_sptr = *thr_ite; // Make a copy of this
            suspended.erase(thr_ite);
            waits.push(thr_sptr);

            thr_sptr->state = thread_state::wait_fast_sema;

            return true;
        }
    }
}