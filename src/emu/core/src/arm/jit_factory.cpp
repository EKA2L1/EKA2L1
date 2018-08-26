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
#include <core/arm/jit_factory.h>
#include <core/arm/jit_unicorn.h>
#include <core/arm/jit_dynarmic.h>

#include <core/core_timing.h>

namespace eka2l1 {
    namespace arm {
        jitter create_jitter(timing_system *timing, manager_system *mngr, memory_system *mem,
            disasm *asmdis, hle::lib_manager *lmngr, jitter_arm_type arm_type) {
            switch (arm_type) {
            case unicorn:
                return std::make_unique<jit_unicorn>(timing, mngr, mem, asmdis, lmngr);
            case dynarmic:
                return std::make_unique<jit_dynarmic>(timing, mngr, mem, asmdis, lmngr);
            default:
                return jitter(nullptr);
            }
        }
    }
}
