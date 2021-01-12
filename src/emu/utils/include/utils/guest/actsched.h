/*
 * Copyright (c) 2021 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <utils/guest/queue.h>
#include <utils/reqsts.h>
#include <mem/ptr.h>

#include <cstdint>

namespace eka2l1::kernel {
    class process;
}

namespace eka2l1::utils {
    struct active_object {
        std::uint32_t vtable_;
        epoc::request_status sts_;
        
        double_queue_link link_;

        void dump(kernel::process *owner);
    };

    struct active_scheduler {
        std::uint32_t vtable_;

        std::uint32_t level_;
        pri_queue act_queue_;

        void dump(kernel::process *owner);
    };
}