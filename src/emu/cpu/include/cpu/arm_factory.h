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

#include <cpu/arm_interface.h>

#include <cstdint>
#include <memory>

namespace eka2l1 {
    namespace arm {
        using core_instance = std::unique_ptr<core>;
        using exclusive_monitor_instance = std::unique_ptr<exclusive_monitor>;

        /**
         * \brief Create a new ARM CPU core.
         * 
         * This factory methods provide various CPU translator backend for you to choose. The CPU must accompanies
         * with other system like kernel or timing, in order to help for emulation.
         * 
         * \returns An instance to the CPU executor.
         */
        core_instance create_core(exclusive_monitor *monitor, arm_emulator_type arm_type);

        exclusive_monitor_instance create_exclusive_monitor(arm_emulator_type arm_type, const std::size_t core_count);
    }
}
