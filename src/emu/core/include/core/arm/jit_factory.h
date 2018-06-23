/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
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

#include <arm/jit_interface.h>
#include <memory>

namespace eka2l1 {
    class timing_system;
    class memory_system;
    class disasm;

    namespace hle {
        class lib_manager;
    }

    namespace arm {
        enum jitter_arm_type {
            unicorn = 0,
            dynarmic = 1
        };

        class jit_interface;
        using jitter = std::unique_ptr<jit_interface>;

        /*! Create a jitter. A JITter is unique by itself. */
        jitter create_jitter(timing_system *timing, memory_system *mem,
            disasm *asmdis, hle::lib_manager *mngr, jitter_arm_type arm_type);
    }
}

