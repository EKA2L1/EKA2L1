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

#pragma once

#include <cpu/12l1r/common.h>
#include <cpu/12l1r/reg_loc.h>

#include <cstdint>

namespace eka2l1::arm::r12l1 {
    class dashixiong_block;

    class float_marker {
    private:
        static constexpr std::uint32_t VFP_REG_COUNT = 32;
        static constexpr std::uint32_t NEON_DOUBLE_REST_REG_COUNT = 16;

        host_register_info single_infos_[VFP_REG_COUNT];
        host_register_info double_simd_infos_[NEON_DOUBLE_REST_REG_COUNT];

        dashixiong_block *big_block_;

    public:
        explicit float_marker(dashixiong_block *bblock);

        void use(const common::armgen::arm_reg reg, const bool read);
        void sync_state(common::armgen::arm_reg state = CORE_STATE_REG, const bool flush = true);
    };

    // This is the algorithm I took from Dynarmic. Thank you guys!
    common::armgen::arm_reg decode_fp_reg(const bool db, const std::size_t base, const bool bit);
}