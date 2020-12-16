/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <common/armemitter.h>
#include <cstdint>

namespace eka2l1::arm::r12l1 {
    enum guest_register_location {
        GUEST_REGISTER_LOC_IMM,                     ///< Guest register is mapped as a constant (used for things like example: PC).
        GUEST_REGISTER_LOC_HOST_REG,                ///< Guest register is currently mapped to a host register
        GUEST_REGISTER_LOC_MEM,                     ///< Guest register is residing in the core state in memory.
        GUEST_REGISTER_LOC_HOST_REG_AS_PTR          ///< Host register containing pointer to this guest register value...
    };

    struct guest_register_info {
        guest_register_location curr_location_;

        union {
            std::uint32_t imm_;
            common::armgen::arm_reg host_reg_;
        };

        bool spill_lock_;
    };

    struct host_register_info {
        common::armgen::arm_reg guest_mapped_reg_;      ///< Correspond guest reigster
        bool dirty_;                                    ///< Value should be written on next flush if this is true
    };

    static constexpr common::armgen::arm_reg CORE_STATE_REG = common::armgen::arm_reg::R10;
    static constexpr common::armgen::arm_reg TICK_COUNT_REG = common::armgen::arm_reg::R7;
}