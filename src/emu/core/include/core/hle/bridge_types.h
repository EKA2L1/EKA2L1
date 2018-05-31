// Vita3K emulator project / EKA2l1 project
// Copyright (C) 2018 Vita3K team / 2018 EKA2l1 team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <ptr.h>

namespace eka2l1 {
    class memory_system;

    namespace hle {
        template <typename host_type>
        host_type arm_to_host(const host_type &t, const memory_system *mem) {
            return t;
        }

        template <typename pointee>
        pointee *arm_to_host(const ptr<pointee> &t, const memory_system *mem) {
            return t.get(mem);
        }

        template <typename arm_type>
        arm_type host_to_arm(const arm_type &t, const memory_system *mem) {
            return t;
        }

        template <typename pointee>
        ptr<pointee> host_to_arm(const address t, const memory_system *mem) {
            return ptr<pointee>(t);
        }
    }
}