/*
 * Copyright (c) 2018 EKA2L1 Team / Citra Team.
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
#include <common/log.h>

#include <core/core_kernel.h>
#include <core/kernel/mutex.h>

namespace eka2l1 {
    namespace kernel {
        mutex::mutex(kernel_system *kern, std::string name, bool init_locked,
            kernel::access_type access)
            : kernel_obj(kern, std::move(name), access)
            , lock_count(0)
            , holding(nullptr) {
            obj_type = object_type::mutex;

            if (init_locked) {
                wait();
            }
        }

        void mutex::wait() {
            if (!holding) {
                holding = kern->crr_thread();

                if (holding->state == thread_state::hold_mutex_pending) {
                    holding->state = thread_state::ready;
                    holding->wait_obj = nullptr;

                    pendings.remove(holding);
                }
            }

            if (holding == kern->crr_thread()) {
                ++lock_count;
            } else {
                assert(!holding->wait_obj);

                waits.push(holding);
                holding->get_scheduler()->wait(holding);
                holding->state = thread_state::wait_mutex;

                holding->wait_obj = this;
            }
        }

        bool mutex::signal() {
            if (!holding) {
                LOG_ERROR("Signal a mutex that's not held by any thread");

                return false;
            }

            --lock_count;

            if (!lock_count) {
                if (waits.empty()) {
                    holding = nullptr;
                    return true;
                }

                thread_ptr ready_thread = std::move(waits.top());
                assert(ready_thread->wait_obj == this);

                waits.pop();

                ready_thread->get_scheduler()->resume(ready_thread);
                ready_thread->state = thread_state::hold_mutex_pending;

                ready_thread->wait_obj = nullptr;

                pendings.push(ready_thread);

                holding = nullptr;
            }

            return true;
        }

        void mutex::wake_next_thread() {
            thread_ptr thr = waits.top();
            waits.pop();

            pendings.push(thr);

            thr->scheduler->resume(thr);
            thr->state = thread_state::hold_mutex_pending;
        }

        void mutex::priority_change(thread *thr) {
            switch (thr->state) {
            case thread_state::hold_mutex_pending: {
                pendings.resort();

                const auto &pending_thr_ite = std::find_if(pendings.begin(), pendings.end(),
                    [thr](const thread_ptr &pending_ct) { return pending_ct.get() == thr; });

                if (pending_thr_ite != pendings.end() && thr->real_priority < waits.top()->real_priority) {
                    // Remove this from pending
                    thread_ptr pending_thr = *pending_thr_ite;
                    pendings.remove(pending_thr);

                    waits.push(pending_thr);
                    waits.resort();

                    pending_thr->get_scheduler()->wait(pending_thr);
                    pending_thr->state = thread_state::wait_mutex;
                }

                break;
            }

            case thread_state::wait_mutex: {
                waits.resort();

                // If the priority is increased, put it in pending
                if (thr->last_priority < thr->real_priority) {
                    const auto &wait_thr_ite = std::find_if(waits.begin(), waits.end(),
                        [thr](const thread_ptr &wait_ct) { return wait_ct.get() == thr; });

                    if (wait_thr_ite != waits.end()) {
                        thread_ptr wait_thr = *wait_thr_ite;
                        waits.remove(wait_thr);
                        pendings.push(wait_thr);

                        wait_thr->get_scheduler()->resume(wait_thr);
                        wait_thr->state = thread_state::hold_mutex_pending;
                    }
                }

                break;
            }

            default: {
                LOG_ERROR("Unknown thread state");
                break;
            }
            }
        }

        bool mutex::suspend_thread(thread *thr) {
            switch (thr->state) {
            case thread_state::wait_mutex: {
                const auto thr_ite = std::find_if(waits.begin(), waits.end(),
                    [thr](const thread_ptr &wait_thr) { return wait_thr.get() == thr; });

                if (thr_ite == waits.end()) {
                    LOG_ERROR("Thread given is not found in waits");
                    return false;
                }

                thread_ptr thr_sptr = *thr_ite; // Make a copy of this
                waits.remove(thr_sptr);
                suspended.push_back(thr_sptr);

                thr_sptr->state = thread_state::wait_mutex_suspend;

                return true;
            }

            case thread_state::hold_mutex_pending: {
                wake_next_thread();
                return false;
            }

            default: {
                LOG_ERROR("Unknown thread state");
                break;
            }
            }

            return false;
        }

        bool mutex::unsuspend_thread(thread *thr) {
            // Putting this thread into another suspend state
            if (thr->current_state() != thread_state::wait_mutex_suspend) {
                LOG_ERROR("Calling mutex to unsuspended a thread that's not suspended");
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

            thr_sptr->state = thread_state::wait_mutex;
        }
    }
}