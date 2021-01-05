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

#include <common/armcommon.h>
#include <cstdint>

namespace eka2l1::arm::r12l1 {
    using vaddress = std::uint32_t;
    using reg_list = std::uint32_t;
    using asid = std::uint8_t;
    using reg_index = std::uint8_t;

    static constexpr std::uint32_t CPSR_THUMB_FLAG_MASK = 0x20;
    static constexpr std::uint32_t CPSR_BIT_POS = 5;

    std::uint32_t expand_arm_imm(std::uint8_t imm, const int rot);
	std::uint32_t expand_arm_shift(std::uint32_t imm, common::armgen::shift_type shift, const std::uint8_t imm5);
}