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

#pragma once

#include <kernel/wait_obj.h>

namespace eka2l1 {
    class memory_system;

    namespace kernel {
        class semaphore : public wait_obj {
            int32_t max_count;
            int32_t avail_count;

        public:
            semaphore(kernel_system *sys, std::string sema_name,
                int32_t init_count,
                int32_t max_count,
                kernel::owner_type own_type,
                kernel::uid own_id,
                kernel::access_type access = access_type::local_access);

            int32_t release(int32_t release_count);

            bool should_wait(kernel::uid thr_id) override;
            void acquire(kernel::uid thr_id) override;
        };
    }
}


