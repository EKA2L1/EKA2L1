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

#include <kernel/kernel.h>
#include <kernel/mutex.h>

namespace eka2l1 {
    namespace kernel {
        mutex::mutex(kernel_system *kern, ntimer *timing, kernel::process *owner, std::string name, bool init_locked,
            kernel::access_type access)
            : kernel_obj(kern, std::move(name), owner, access)
            , timing(timing)
            , lock_count(0)
            , holding(nullptr)
            , suspend_count(0)
            , wait_for_timeout(false) {
            obj_type = object_type::mutex;

            mutex_event_type = timing->register_event("MutexWaking" + common::to_string(uid),
                std::bind(&mutex::waking_up_from_suspension, this, std::placeholders::_1, std::placeholders::_2));

            if (init_locked) {
                wait(kern->crr_thread());
            }
        }

        int mutex::destroy() {
            while (!pendings.empty()) {
                thread *thr = E_LOFF(pendings.first()->deque(), thread, pending_link);
                thr->state = thread_state::ready;
                thr->wait_obj = nullptr;
            }

            while (!suspended.empty()) {
                thread *thr = E_LOFF(suspended.first()->deque(), thread, suspend_link);
                thr->wait_obj = nullptr;
                thr->get_scheduler()->dewait(thr);
            }

            timing->remove_event(mutex_event_type);

            kernel::process *own = reinterpret_cast<kernel::process*>(owner);
            if (own)
                own->decrease_access_count();

            return 0;
        }

        void mutex::wait(thread *thr) {
            if (!holding) {
                holding = thr;

                if (holding->state == thread_state::hold_mutex_pending) {
                    holding->state = thread_state::ready;
                    holding->wait_obj = nullptr;

                    holding->pending_link.deque();
                }
            }

            if (holding == thr) {
                ++lock_count;
            } else {
                assert(!holding->wait_obj);

                kernel::thread *calm_down = thr;
                waits.push(calm_down);

                calm_down->get_scheduler()->wait(calm_down);
                calm_down->state = thread_state::wait_mutex;

                calm_down->wait_obj = this;
            }
        }

        void mutex::try_wait(thread *thr) {
            if (!holding) {
                holding = thr;

                if (holding->state == thread_state::hold_mutex_pending) {
                    holding->state = thread_state::ready;
                    holding->wait_obj = nullptr;

                    holding->pending_link.deque();
                }
            }

            if (holding == thr) {
                lock_count++;
            }
        }

        void mutex::waking_up_from_suspension(std::uint64_t userdata, int cycles_late) {
            kernel::thread *thread_to_wake = reinterpret_cast<kernel::thread *>(userdata);
            kernel_system *kern = thread_to_wake->get_kernel_object_owner();

            kern->lock();

            if (!wait_for_timeout) {
                kern->unlock();
                return;
            }
            
            wait_for_timeout = false;

            if (!thread_to_wake || thread_to_wake->get_object_type() != kernel::object_type::thread) {
                LOG_ERROR(KERNEL, "Waking up invalid object!!!!!");
                kern->unlock();
                return;
            }

            switch (thread_to_wake->current_state()) {
            case thread_state::hold_mutex_pending: {
                if (thread_to_wake->pending_link.alone()) {
                    LOG_ERROR(KERNEL, "Thread request to wake up with this mutex is not in hold pending queue"); 
                    kern->unlock();
                    return;
                }

                thread_to_wake->pending_link.deque();

                break;
            }

            case thread_state::wait_mutex: {
                auto thr_ite = std::find(waits.begin(), waits.end(), thread_to_wake);

                if (thr_ite == waits.end()) {
                    LOG_ERROR(KERNEL, "Thread request to wake up with this mutex is not in hold pending queue");
                    kern->unlock();

                    return;
                }

                waits.remove(thread_to_wake);
                break;
            }

            default: {
                LOG_ERROR(KERNEL, "Unknown thread state to wake up");
                kern->unlock();
                return;
            }
            }

            if (thread_to_wake->wait_obj == this) {
                thread_to_wake->wait_obj = nullptr;
            }

            thread_to_wake->scheduler->dewait(thread_to_wake);
            kern->unlock();
        }

        void mutex::wait_for(int usecs) {
            bool should_schedule = false;
            if (!holding) {
                should_schedule = true;
            }

            wait(kern->crr_thread());

            // Schedule event to wake up
            if (should_schedule) {
                wait_for_timeout = true;
                timing->schedule_event(usecs, mutex_event_type, reinterpret_cast<std::uint64_t>(kern->crr_thread()));
            }
        }

        bool mutex::signal(kernel::thread *callee) {
            if (!holding) {
                LOG_ERROR(KERNEL, "Signal a mutex that's not held by any thread");
                return false;
            }

            if (holding != callee) {
                LOG_ERROR(KERNEL, "Calling signal with the caller not being the holder");
                return false;
            }

            --lock_count;

            auto put_top_wait_to_pending = [&]() {
                if (waits.empty()) {
                    return;
                }

                // Take it from top of the wait queue
                kernel::thread *top_wait = std::move(waits.top());
                waits.pop();

                pendings.push(&top_wait->pending_link);

                assert(top_wait->wait_obj == this);

                if (wait_for_timeout) {
                    timing->unschedule_event(mutex_event_type, reinterpret_cast<std::uint64_t>(top_wait));
                    wait_for_timeout = false;
                }

                top_wait->state = kernel::thread_state::hold_mutex_pending;
            };

            if (lock_count <= 0) {
                if (!pendings.empty()) {
                    // Make the pending thread hold the mutex
                    auto elem = pendings.first();
                    kernel::thread *thr = E_LOFF(elem->deque(), thread, pending_link);

                    // dewait from wait, directly use scheduler
                    thr->get_scheduler()->dewait(thr);
                    thr->state = thread_state::ready;
                    thr->wait_obj = nullptr;

                    holding = thr;

                    // Try to get a thread to be in a mutex holding ready state (mutex hold pending)
                    if (pendings.empty()) {
                        put_top_wait_to_pending();
                    }

                    return true;
                }

                if (waits.empty()) {
                    // Nothing nothing nothing nothing....
                    holding = nullptr;
                    return true;
                }

                // The pending queue is currently empty, we might need to kickstart it
                kernel::thread *ready_thread = std::move(waits.top());
                waits.pop();

                ready_thread->get_scheduler()->dewait(ready_thread);
                ready_thread->state = thread_state::ready;
                ready_thread->wait_obj = nullptr;

                // Push the next thread in wait queue to the pending queue
                holding = ready_thread;

                if (!waits.empty()) {
                    put_top_wait_to_pending();
                }
            }

            return true;
        }

        void mutex::wake_next_thread() {
            kernel::thread *thr = waits.top();
            waits.pop();

            pendings.push(&thr->pending_link);

            thr->scheduler->dewait(thr);
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

                        thr->get_scheduler()->dewait(thr);
                        thr->state = thread_state::hold_mutex_pending;
                    }
                }

                break;
            }

            default: {
                LOG_ERROR(KERNEL, "Unknown thread state");
                break;
            }
            }
        }

        void mutex::transfer_suspend_from_condvar(thread *thr) {
            suspended.push(&thr->suspend_link);

            suspend_count++;
            thr->state = thread_state::wait_mutex_suspend;
            thr->wait_obj = this;
        }

        bool mutex::suspend_thread(thread *thr) {
            switch (thr->state) {
            case thread_state::wait_mutex: {
                const auto thr_ite = std::find(waits.begin(), waits.end(), thr);

                if (thr_ite == waits.end()) {
                    LOG_ERROR(KERNEL, "Thread given is not found in waits");
                    return false;
                }

                waits.remove(thr);
                suspended.push(&thr->suspend_link);

                suspend_count++;
                thr->state = thread_state::wait_mutex_suspend;

                return true;
            }

            case thread_state::hold_mutex_pending: {
                wake_next_thread();
                return false;
            }

            default: {
                LOG_ERROR(KERNEL, "Unknown thread state");
                break;
            }
            }

            return false;
        }

        bool mutex::unsuspend_thread(thread *thr) {
            // Putting this thread into another suspend state
            if (thr->current_state() != thread_state::wait_mutex_suspend) {
                LOG_ERROR(KERNEL, "Calling mutex to unsuspended a thread that's not suspended");
                return false;
            }

            if (thr->suspend_link.alone()) {
                LOG_ERROR(KERNEL, "Thread given is not found in suspended");
                return false;
            }

            thr->suspend_link.deque();
            waits.push(thr);

            suspend_count--;
            thr->state = thread_state::wait_mutex;

            return true;
        }

        int mutex::count() const {
            return static_cast<int>(waits.size() + suspend_count);
        }
    }
}
