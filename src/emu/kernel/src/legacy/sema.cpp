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

#include <kernel/legacy/sema.h>

namespace eka2l1::kernel::legacy {
    semaphore::semaphore(kernel_system *kern, const std::string sema_name, const std::int32_t initial_count,
        kernel::access_type access)
        : sync_object_base(kern, sema_name, initial_count, access) {
        obj_type = kernel::object_type::sema;
    }

    void semaphore::wait() {
        wait_impl(thread_state::wait_fast_sema);
    }

    void semaphore::signal(const std::int32_t count) {
        signal_impl(count);
    }

    bool semaphore::suspend_waiting_thread(thread *thr) {
        return suspend_waiting_thread_impl(thr, thread_state::wait_fast_sema_suspend);
    }

    bool semaphore::unsuspend_waiting_thread(thread *thr) {
        return unsuspend_waiting_thread_impl(thr, thread_state::wait_fast_sema);
    }
}