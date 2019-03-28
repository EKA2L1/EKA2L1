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
            : kernel_obj(kern, std::move(name), access)
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

        void mutex::try_wait() {
            if (!holding) {
                holding = kern->crr_thread();

                if (holding->state == thread_state::hold_mutex_pending) {
                    holding->state = thread_state::ready;
                    holding->wait_obj = nullptr;

                    pendings.remove(holding);
                }
            }

            if (holding == kern->crr_thread()) {
                lock_count++;
            }
        }

        void mutex::waking_up_from_suspension(std::uint64_t userdata, int cycles_late) {
            kernel::thread *thread_to_wake = reinterpret_cast<kernel::thread *>(userdata);

            if (!thread_to_wake || thread_to_wake->get_object_type() != kernel::object_type::thread) {
                LOG_ERROR("Waking up invalid object!!!!!");
                return;
            }

            switch (thread_to_wake->current_state()) {
            case thread_state::hold_mutex_pending: {
                auto thr_ite = std::find(pendings.begin(), pendings.end(), thread_to_wake);

                if (thr_ite == pendings.end()) {
                    LOG_ERROR("Thread request to wake up with this mutex is not in hold pending queue");
                    return;
                }

                pendings.remove(thread_to_wake);

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
            if (!holding) {
                LOG_ERROR("Signal a mutex that's not held by any thread");

                return false;
            }

            if (holding != callee) {
                LOG_ERROR("Calling signal with the caller not being the holder");
                return false;
            }

            --lock_count;

            if (!lock_count) {
                if (waits.empty()) {
                    holding = nullptr;
                    return true;
                }

                kernel::thread *ready_thread = std::move(waits.top());
                assert(ready_thread->wait_obj == this);

                timing->unschedule_event(mutex_event_type, reinterpret_cast<std::uint64_t>(ready_thread));
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
            kernel::thread *thr = waits.top();
            waits.pop();

            pendings.push(thr);

            thr->scheduler->resume(thr);
            thr->state = thread_state::hold_mutex_pending;
        }

        void mutex::priority_change(thread *thr) {
            switch (thr->state) {
            case thread_state::hold_mutex_pending: {
                pendings.resort();

                const auto &pending_thr_ite = std::find(pendings.begin(), pendings.end(), thr);

                if (pending_thr_ite != pendings.end() && thr->real_priority < waits.top()->real_priority) {
                    // Remove this from pending
                    pendings.remove(thr);

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
                        pendings.push(thr);

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
                suspended.push_back(thr);

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

            const auto thr_ite = std::find(suspended.begin(), suspended.end(), thr);

            if (thr_ite == waits.end()) {
                LOG_ERROR("Thread given is not found in suspended");
                return false;
            }

            suspended.erase(thr_ite);
            waits.push(thr);

            thr->state = thread_state::wait_mutex;

            return true;
        }

        void mutex::do_state(common::chunkyseri &seri) {
            std::uint32_t holding_id = (holding ? holding->unique_id() : 0);
            seri.absorb(holding_id);

            if (seri.get_seri_mode() == common::SERI_MODE_WRITE) {
                holding = &(*kern->get<kernel::thread>(holding_id));
            }

            std::uint32_t total_waits = static_cast<std::uint32_t>(waits.size());
            seri.absorb(total_waits);

            for (size_t i = 0; i < total_waits; i++) {
                std::uint32_t wait_thr_id = (waits.empty() ? 0 : (*(waits.begin() + i))->unique_id());
                seri.absorb(wait_thr_id);

                if (seri.get_seri_mode() == common::SERI_MODE_WRITE) {
                    waits.push(&(*kern->get<kernel::thread>(wait_thr_id)));
                }
            }

            std::uint32_t total_suspended = static_cast<std::uint32_t>(suspended.size());
            seri.absorb(total_suspended);

            for (size_t i = 0; i < total_suspended; i++) {
                std::uint32_t sus_thr_id;
                seri.absorb(sus_thr_id);

                if (seri.get_seri_mode() == common::SERI_MODE_WRITE) {
                    suspended.push_back(&(*kern->get<kernel::thread>(sus_thr_id)));
                }
            }
        }
    }
}
