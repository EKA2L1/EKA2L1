/*
 * Copyright (c) 2021 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <kernel/legacy/sync_object.h>
#include <kernel/kernel.h>

#include <common/log.h>

namespace eka2l1::kernel::legacy {
    sync_object_base::sync_object_base(kernel_system *kern, std::string name, std::int32_t init_count, kernel::access_type access)
        : kernel_obj(kern, std::move(name), kern->crr_process(), access)
        , count_(init_count) {
    }

    void sync_object_base::signal_impl(std::int32_t signal_count) {
        std::int32_t prev_count = count_;

        for (size_t i = 0; i < signal_count; i++) {
            if (count_++ < 0) {
                if (!waits_.empty()) {
                    common::double_linked_queue_element *ready_thread_link = waits_.first();
                    kernel::thread *ready_thread = E_LOFF(ready_thread_link, kernel::thread, wait_link);

                    assert(ready_thread->wait_obj == this);

                    ready_thread->get_scheduler()->dewait(ready_thread);
                    ready_thread->wait_obj = nullptr;

                    // Remove from wait list
                    ready_thread_link->deque();
                }
            }
        }
    }

    void sync_object_base::wait_impl(thread_state target_wait_state) {
        kernel::thread *calling_thr = kern->crr_thread();

        if (--count_ < 0) {
            assert(!calling_thr->wait_obj);

            waits_.push(&calling_thr->wait_link);
            calling_thr->get_scheduler()->wait(calling_thr);

            calling_thr->state = target_wait_state;
            calling_thr->wait_obj = this;
        }
    }

    bool sync_object_base::suspend_waiting_thread_impl(thread *thr, thread_state target_suspend_state) {
        // Putting this thread into another suspend state
        if ((thr->current_state() != thread_state::wait_fast_sema) && (thr->current_state() != thread_state::wait_mutex)) {
            LOG_ERROR(KERNEL, "Calling sync object to suspend a thread that's not waiting");
            return false;
        }

        thr->wait_link.deque();
        suspended_.push(&thr->suspend_link);

        thr->state = target_suspend_state;
        return true;
    }

    bool sync_object_base::unsuspend_waiting_thread_impl(thread *thr, thread_state target_wait_state) {
        // Putting this thread into another suspend state
        if ((thr->current_state() != thread_state::wait_fast_sema_suspend) && (thr->current_state() != thread_state::wait_mutex_suspend)) {
            LOG_ERROR(KERNEL, "Calling sync object to unsuspended a thread that's not suspended");
            return false;
        }

        if (thr->suspend_link.alone()) {
            LOG_ERROR(KERNEL, "Thread given is not found in suspended");
            return false;
        }

        thr->suspend_link.deque();
        waits_.push(&thr->wait_link);

        thr->state = target_wait_state;

        return true;
    }
}