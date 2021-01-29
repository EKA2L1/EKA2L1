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

#include <kernel/legacy/sync_object.h>

namespace eka2l1::kernel::legacy {
    class semaphore: public sync_object_base {
    public:
        explicit semaphore(kernel_system *kern, const std::string sema_name, const std::int32_t initial_count,
            kernel::access_type access = access_type::local_access);
        
        void wait();
        void signal(const std::int32_t count);

        bool suspend_waiting_thread(thread *thr);
        bool unsuspend_waiting_thread(thread *thr);
    };
}