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

#include <arm/arm_interface.h>
#include <memory>

namespace eka2l1 {
    class kernel_system;
    class timing_system;
    class memory_system;
    class manager_system;

    class disasm;
    class gdbstub;

    namespace hle {
        class lib_manager;
    }

    namespace manager {
        struct config_state;
    }

    namespace arm {
        class arm_interface;
        using jitter = std::unique_ptr<arm_interface>;

        /*! Create a jitter. A JITter is unique by itself. */
        jitter create_jitter(kernel_system *kern, timing_system *timing, manager::config_state *conf, manager_system *mngr, 
            memory_system *mem, disasm *asmdis, hle::lib_manager *lmngr, gdbstub *stub, debugger_ptr debugger, arm_emulator_type arm_type);
    }
}
