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

#include <kernel/legacy/mutex.h>
#include <kernel/kernel.h>

#include <common/log.h>

namespace eka2l1::kernel::legacy {
    mutex::mutex(kernel_system *kern, const std::string mut_name, kernel::access_type access)
        : sync_object_base(kern, mut_name, 1, access)
        , holding_(nullptr)
        , hold_count_(0) {
        obj_type = kernel::object_type::mutex;
    }

    void mutex::wait() {
        kernel::thread *crr_thread = kern->crr_thread();
        if (!holding_) {
            holding_ = crr_thread;
            hold_count_++;
        } else {
            if (holding_ == crr_thread) {
                hold_count_++;
                return;
            }
        }

        wait_impl(thread_state::wait_mutex);
    }

    void mutex::signal() {
        kernel::thread *crr_thread = kern->crr_thread();
        if (holding_ == crr_thread) {
            if (--hold_count_ == 0) {
                holding_ = nullptr;
            } else {
                return;
            }
        }

        signal_impl(1);
    }

    bool mutex::suspend_waiting_thread(thread *thr) {
        return suspend_waiting_thread_impl(thr, thread_state::wait_mutex_suspend);
    }

    bool mutex::unsuspend_waiting_thread(thread *thr) {
        return unsuspend_waiting_thread_impl(thr, thread_state::wait_mutex);
    }
}