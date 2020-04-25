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
#include <arm/arm_dynarmic.h>
#include <arm/arm_factory.h>
#include <arm/arm_unicorn.h>

#include <epoc/timing.h>

namespace eka2l1::arm {
    cpu create_cpu(kernel_system *kern, timing_system *timing, manager::config_state *conf,
        manager_system *mngr, memory_system *mem, disasm *asmdis, hle::lib_manager *lmngr, gdbstub *stub,
        arm_emulator_type arm_type) {
        switch (arm_type) {
        case unicorn:
            return std::make_unique<arm_unicorn>(kern, timing, conf, mngr, mem, asmdis, lmngr, stub);
        case dynarmic:
            return std::make_unique<arm_dynarmic>(kern, timing, conf, mngr, mem, asmdis, lmngr, stub);
        default:
            break;
        }

        return nullptr;
    }
}