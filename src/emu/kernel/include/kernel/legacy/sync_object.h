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

#pragma once

#include <common/linked.h>
#include <kernel/kernel_obj.h>
#include <kernel/thread.h>

namespace eka2l1::kernel::legacy {
    class sync_object_base: public kernel_obj {
    private:
        common::roundabout waits_;
        common::roundabout suspended_;

        std::int32_t count_;

    protected:
        void wait_impl(thread_state target_wait_state);

        bool suspend_waiting_thread_impl(thread *thr, thread_state target_suspend_state);
        bool unsuspend_waiting_thread_impl(thread *thr, thread_state target_wait_state);

        void signal_impl(std::int32_t signal_count);

    public:
        explicit sync_object_base(kernel_system *kern, const std::string name, std::int32_t init_count,
            kernel::access_type access = access_type::local_access);

        std::int32_t count() const {
            return count_;
        }
    };
}