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
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/log.h>

#include <epoc/kernel.h>
#include <epoc/kernel/mutex.h>

namespace eka2l1 {
    namespace kernel {
        mutex::mutex(kernel_system *kern, timing_system *timing, std::string name, bool init_locked,
            kernel::access_type access)
            : kernel_obj(kern, std::move(name), kern->crr_process(), access)
            , timing(timing)
            , lock_count(0)
            , holding(nullptr) {
            obj_type = object_type::mutex;

            mutex_event_type = timing->register_event("MutexWaking" + common::to_string(uid),
                std::bind(&mutex::waking_up_from_suspension, this, std::placeholders::_1, std::placeholders::_2));

            if (init_locked) {
                wait();
            }
        }

        void mutex::destroy() {
            while (!pendings.empty()) {
                thread *thr = E_LOFF(pendings.first()->deque(), thread, pending_link);
                thr->state = thread_state::ready;
                thr->wait_obj = nullptr;
            }

            while (!suspended.empty()) {
                thread *thr = E_LOFF(suspended.first()->deque(), thread, suspend_link);
                thr->wait_obj = nullptr;
                thr->get_scheduler()->resume(thr);
            }
        }

        void mutex::wait() {
            kern->lock();

            if (!holding) {
                holding = kern->crr_thread();

                if (holding->state == thread_state::hold_mutex_pending) {
                    holding->state = thread_state::ready;
                    holding->wait_obj = nullptr;

                    holding->pending_link.deque();
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

            kern->unlock();
        }

        void mutex::try_wait() {
            kern->lock();

            if (!holding) {
                holding = kern->crr_thread();

                if (holding->state == thread_state::hold_mutex_pending) {
                    holding->state = thread_state::ready;
                    holding->wait_obj = nullptr;

                    holding->pending_link.deque();
                }
            }

            if (holding == kern->crr_thread()) {
                lock_count++;
            }

            kern->unlock();
        }

        void mutex::waking_up_from_suspension(std::uint64_t userdata, int cycles_late) {
            kernel::thread *thread_to_wake = reinterpret_cast<kernel::thread *>(userdata);

            if (!thread_to_wake || thread_to_wake->get_object_type() != kernel::object_type::thread) {
                LOG_ERROR("Waking up invalid object!!!!!");
                return;
            }

            switch (thread_to_wake->current_state()) {
            case thread_state::hold_mutex_pending: {
                if (thread_to_wake->pending_link.alone()) {
                    LOG_ERROR("Thread request to wake up with this mutex is not in hold pending queue");
                    return;
                }

                thread_to_wake->pending_link.deque();

                break;
            }

            case thread_state::wait_mutex: {
                auto thr_ite = std::find(waits.begin(), waits.end(), thread_to_wake);
                
                if (thr_ite == waits.end()) {
                    LOG_ERROR("Thread request to wake up with this mutex is not in hold pending queue");
                    return;
                }

                waits.remove(thread_to_wake);
                break;
            }

            default: {
                LOG_ERROR("Unknown thread state to wake up");
                return;
            }
            }

            thread_to_wake->resume();
        }

        void mutex::wait_for(int msecs) {
            wait();

            // Schedule event to wake up
            timing->schedule_event(timing->ms_to_cycles(msecs), mutex_event_type,
                reinterpret_cast<std::uint64_t>(kern->crr_thread()));
        }

        bool mutex::signal(kernel::thread *callee) {
            kern->lock();

            if (!holding) {
                LOG_ERROR("Signal a mutex that's not held by any thread");
                kern->unlock();
                return false;
            }

            if (holding != callee) {
                LOG_ERROR("Calling signal with the caller not being the holder");
                kern->unlock();
                return false;
            }

            --lock_count;

            if (!lock_count) {
                if (waits.empty()) {
                    holding = nullptr;
                    kern->unlock();
                    return true;
                }

                kernel::thread *ready_thread = std::move(waits.top());
                assert(ready_thread->wait_obj == this);

                timing->unschedule_event(mutex_event_type, reinterpret_cast<std::uint64_t>(ready_thread));
                waits.pop();

                ready_thread->get_scheduler()->resume(ready_thread);
                ready_thread->state = thread_state::hold_mutex_pending;
                ready_thread->wait_obj = this;

                pendings.push(&ready_thread->pending_link);

                holding = nullptr;
            }

            kern->unlock();
            return true;
        }

        void mutex::wake_next_thread() {
            kernel::thread *thr = waits.top();
            waits.pop();

            pendings.push(&thr->pending_link);

            thr->scheduler->resume(thr);
            thr->state = thread_state::hold_mutex_pending;
        }

        void mutex::priority_change(thread *thr) {
            switch (thr->state) {
            case thread_state::hold_mutex_pending: {
                if (!thr->pending_link.alone() && thr->real_priority < waits.top()->real_priority) {
                    // Remove this from pending
                    thr->pending_link.deque();

                    waits.push(thr);
                    waits.resort();

                    thr->get_scheduler()->wait(thr);
                    thr->state = thread_state::wait_mutex;
                }

                break;
            }

            case thread_state::wait_mutex: {
                waits.resort();

                // If the priority is increased, put it in pending
                if (thr->last_priority < thr->real_priority) {
                    const auto &wait_thr_ite = std::find(waits.begin(), waits.end(), thr);

                    if (wait_thr_ite != waits.end()) {
                        waits.remove(thr);
                        pendings.push(&thr->pending_link);

                        thr->get_scheduler()->resume(thr);
                        thr->state = thread_state::hold_mutex_pending;
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
                const auto thr_ite = std::find(waits.begin(), waits.end(), thr);

                if (thr_ite == waits.end()) {
                    LOG_ERROR("Thread given is not found in waits");
                    return false;
                }

                waits.remove(thr);
                suspended.push(&thr->suspend_link);

                thr->state = thread_state::wait_mutex_suspend;

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

            if (thr->suspend_link.alone()) {
                LOG_ERROR("Thread given is not found in suspended");
                return false;
            }

            thr->suspend_link.deque();
            waits.push(thr);

            thr->state = thread_state::wait_mutex;

            return true;
        }
    }
}
