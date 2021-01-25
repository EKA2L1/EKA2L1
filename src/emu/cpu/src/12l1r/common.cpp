/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <common/bytes.h>
#include <cpu/12l1r/common.h>

#include <cassert>

namespace eka2l1::arm::r12l1 {
    std::uint32_t expand_arm_imm(std::uint8_t imm, const int rot) {
        return common::rotate_right<std::uint32_t>(static_cast<std::uint32_t>(imm), rot * 2);
    }

    std::uint32_t expand_arm_shift(std::uint32_t imm, common::armgen::shift_type shift, const std::uint8_t imm5) {
        switch (shift) {
        case common::armgen::ST_LSL:
            return imm << imm5;

        case common::armgen::ST_LSR:
            return imm >> (imm5 ? imm5 : 32);

        default:
            assert(false);
            break;
        }

        return 0;
    }
}
