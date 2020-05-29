/*
* Copyright (c) 2018 Vita3K Team / EKA2L1 Team.
*
* This file is part of Vita3K emulator project / EKA2L1 project
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

#include <mem/ptr.h>

namespace eka2l1 {
    namespace kernel {
        class process;
    }

    namespace hle {
        template <typename host_type>
        struct bridge_type {
            typedef host_type arm_type;

            static host_type arm_to_host(const arm_type &t, kernel::process *pr) {
                return t;
            }

            static arm_type host_to_arm(const host_type &t, kernel::process *pr) {
                return t;
            }
        };

        template <typename pointee>
        struct bridge_type<pointee *> {
            typedef ptr<pointee> arm_type;

            static pointee *arm_to_host(const arm_type &t, kernel::process *pr) {
                return t.get(pr);
            }
        };
    }
}
