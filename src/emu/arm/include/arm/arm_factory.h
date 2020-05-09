/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <arm/arm_interface.h>
#include <memory>

namespace eka2l1 {
    class kernel_system;
    class ntimer;
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
        using cpu = std::unique_ptr<arm_interface>;

        /**
         * \brief Create a new ARM CPU executor.
         * 
         * This factory methods provide various CPU translator backend for you to choose. The CPU must accompanies
         * with other system like kernel or timing, in order to help for emulation.
         * 
         * \returns An instance to the CPU executor.
         */
        cpu create_cpu(kernel_system *kern, ntimer *timing, manager::config_state *conf, manager_system *mngr,
            memory_system *mem, disasm *asmdis, hle::lib_manager *lmngr, gdbstub *stub, arm_emulator_type arm_type);
    }
}
