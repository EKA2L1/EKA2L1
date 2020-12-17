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
        GUEST_REGISTER_LOC_IMM = 1 << 0,                     ///< Guest register is mapped as a constant (used for things like example: PC).
        GUEST_REGISTER_LOC_HOST_REG = 1 << 1,                ///< Guest register is currently mapped to a host register
        GUEST_REGISTER_LOC_MEM = 1 << 2,                     ///< Guest register is residing in the core state in memory.
        GUEST_REGISTER_LOC_IMM_AND_HOST_REG = GUEST_REGISTER_LOC_IMM | GUEST_REGISTER_LOC_HOST_REG
    };

    struct guest_register_info {
        guest_register_location curr_location_;

        union {
            std::uint32_t imm_;
            common::armgen::arm_reg host_reg_;
        };

        bool spill_lock_;
        std::uint32_t use_count_;

        explicit guest_register_info();
    };

    struct host_register_info {
        common::armgen::arm_reg guest_mapped_reg_;      ///< Correspond guest reigster
        bool scratch_;                                  ///< This host register should not be touched until the scratch is release
        bool dirty_;                                    ///< No need to flush this register if this is true. May be used for reading purposes.

        explicit host_register_info();
    };

    static constexpr common::armgen::arm_reg CORE_STATE_REG = common::armgen::arm_reg::R10;
    static constexpr common::armgen::arm_reg TLB_ENTRIES_REG = common::armgen::arm_reg::R9;
    static constexpr common::armgen::arm_reg ALWAYS_SCRATCH1 = common::armgen::arm_reg::R0;
    static constexpr common::armgen::arm_reg ALWAYS_SCRATCH2 = common::armgen::arm_reg::R14;

    static common::armgen::arm_reg ALLOCATEABLE_GPRS[] = {
        common::armgen::R1, common::armgen::R2, common::armgen::R3, common::armgen::R4,
        common::armgen::R5, common::armgen::R6, common::armgen::R7, common::armgen::R8,
        common::armgen::R12
    };
}