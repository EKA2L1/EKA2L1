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

#include <common/log.h>

#include <kernel/mutex.h>
#include <core_kernel.h>

namespace eka2l1 {
    namespace kernel {
        mutex::mutex(kernel_system *kern, std::string name, bool init_locked,
            kernel::owner_type own,
            kernel::uid own_id,
            kernel::access_type access)
            : wait_obj(kern, std::move(name), own, own_id, access) 
             , lock_count(0) {
            obj_type = object_type::mutex;

            if (init_locked) {
                acquire(kern->crr_thread()->unique_id());
            }
        }

        void mutex::update_priority() {
            if (!holding) {
                return;
            }

            int best_priority = 10000;

            for (auto& waiter : waiting_threads()) {
                if (waiter->current_priority() < best_priority) {
                    best_priority = waiter->current_priority();
                }
            }

            if (best_priority != priority) {
                priority = best_priority;
                holding->update_priority();
            }
        }

        bool mutex::should_wait(kernel::uid thr_id) {
            // If the mutex is acquired and the given thread is not in hold
            return lock_count > 0 && holding->unique_id() != thr_id;
        }

        void mutex::acquire(kernel::uid thr_id) {
            kernel_obj_ptr kobj = kern->get_kernel_obj(thr_id);

            if (kobj) {
                thread_ptr thr_ptr = std::reinterpret_pointer_cast<thread>(kobj);

                if (thr_ptr && lock_count == 0) {
                    priority = thr_ptr->current_priority();

                    // TODO: make this less ugly
                    thr_ptr->held_mutexes.push_back(std::reinterpret_pointer_cast<mutex>(kern->get_kernel_obj(obj_id)));
                    holding = thr_ptr;

                    // boost
                    thr_ptr->update_priority();
                }
            }
        }

        bool mutex::release(thread_ptr thr) {
            if (holding && thr != holding) {
                return false;
            }

            if (lock_count <= 0) {
                LOG_WARN("Release a mutex that doesn't being held by any thread");
                return false;
            }

            lock_count--;

            if (lock_count == 0) {
                auto res = std::find_if(thr->held_mutexes.begin(), thr->held_mutexes.end(),
                    [&](auto mut) { return mut->unique_id() == obj_id; });

                if (res != thr->held_mutexes.end()) {
                    holding->held_mutexes.erase(res);
                    holding->update_priority();
                    holding = nullptr;

                    return true;
                }

                LOG_WARN("Thread doesn't held this mutex, weird");
            }

            return false;
        }

        void mutex::add_waiting_thread(thread_ptr thr) {
            wait_obj::add_waiting_thread(thr);
            thr->pending_mutexes.push_back(std::reinterpret_pointer_cast<mutex>(kern->get_kernel_obj(obj_id)));
            update_priority();
        }

        void mutex::erase_waiting_thread(kernel::uid thr) {
            wait_obj::erase_waiting_thread(thr);
            kernel_obj_ptr kobj = kern->get_kernel_obj(thr);

            if (kobj) {
                thread_ptr thr_ptr = std::reinterpret_pointer_cast<thread>(kobj);

                if (thr_ptr) {
                    auto res = std::find_if(thr_ptr->pending_mutexes.begin(), thr_ptr->pending_mutexes.begin(),
                        [&](auto mut) { return mut->unique_id() == obj_id; });

                    if (res != thr_ptr->pending_mutexes.end()) {
                        thr_ptr->pending_mutexes.erase(res);
                        update_priority();
                    }
                }
            }
        }
    }
}