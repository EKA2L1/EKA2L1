/*
* Copyright (c) 2018 EKA2L1 Team / Citra Team
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

#include <kernel/sema.h>

namespace eka2l1 {
    namespace kernel {
        semaphore::semaphore(kernel_system *sys, std::string sema_name,
            int32_t init_count,
            int32_t max_count,
            kernel::owner_type own_type,
            kernel::uid own_id,
            kernel::access_type access)
            : wait_obj(sys, std::move(sema_name), own_type, own_id, access),
              avail_count(init_count),
              max_count(max_count) {
            obj_type = object_type::sema;
        }

        int32_t semaphore::release(int32_t release_count) {
            if (avail_count > 0 || max_count - avail_count < release_count) {
                return -1;
            }

            int32_t prev_count = avail_count;
            avail_count += release_count;

            wait_obj::wake_up_waiting_threads();

            return prev_count;
        }

        bool semaphore::should_wait(kernel::uid thr_id) {
            return avail_count <= 0;
        }

        void semaphore::acquire(kernel::uid thr_id) {
            if (avail_count <= 0) {
                return;
            }

            --avail_count;
        }
    }
}