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

#include <cpu/12l1r/block_gen.h>
#include <cpu/12l1r/float_marker.h>
#include <cpu/12l1r/core_state.h>

namespace eka2l1::arm::r12l1 {
    float_marker::float_marker(dashixiong_block *bblock)
        : big_block_(bblock) {
    }

    void float_marker::use(const common::armgen::arm_reg reg, const std::uint32_t use_flags) {
        int reg_index = 0;
        int consecutive = 1;

        bool load_single = true;

        if ((reg >= common::armgen::S0) && (reg <= common::armgen::S31)) {
            reg_index = static_cast<int>(reg) - static_cast<int>(common::armgen::S0);
            load_single = true;
            consecutive = 1;
        } else if ((reg >= common::armgen::D0) && (reg <= common::armgen::D15)) {
            // This is because a component of D register in here maybe already loaded
            // For example, when trying to use D2, S4 already loaded and dirtied...
            reg_index = (static_cast<int>(reg) - static_cast<int>(common::armgen::D0)) * 2;
            load_single = true;
            consecutive = 2;
        } else if ((reg >= common::armgen::Q0) && (reg <= common::armgen::Q7)) {
            reg_index = (static_cast<int>(reg) - static_cast<int>(common::armgen::Q0)) * 4;
            load_single = true;
            consecutive = 4;
        } else if ((reg >= common::armgen::D16) && (reg <= common::armgen::D31)) {
            reg_index = static_cast<int>(reg) - static_cast<int>(common::armgen::D16);
            load_single = false;
            consecutive = 1;
        } else if ((reg >= common::armgen::Q8) && (reg <= common::armgen::Q15)) {
            reg_index = (static_cast<int>(reg) - static_cast<int>(common::armgen::Q8)) * 2;
            load_single = false;
            consecutive = 2;
        }

        for (int i = 0; i < consecutive; i++) {
            common::armgen::arm_reg base_load = common::armgen::INVALID_REG;
            int offset_from_state = 0;

            // Scratch here means loaded
            if (load_single) {
                if ((use_flags & FLOAT_MARKER_USE_READ) && !single_infos_[reg_index + i].scratch_ && !single_infos_[reg_index + i].dirty_) {
                    base_load = static_cast<common::armgen::arm_reg>(common::armgen::S0 + reg_index + i);
                    offset_from_state = offsetof(core_state, fprs_[0]) + sizeof(std::uint32_t) * (reg_index + i);

                    single_infos_[reg_index + i].scratch_ = true;
                }

                if ((use_flags & FLOAT_MARKER_USE_WRITE) && !single_infos_[reg_index + i].dirty_) {
                    single_infos_[reg_index + i].dirty_ = true;
                }
            } else {
                if ((use_flags & FLOAT_MARKER_USE_READ) && !double_simd_infos_[reg_index + i].scratch_ && !double_simd_infos_[reg_index + i].dirty_) {
                    base_load = static_cast<common::armgen::arm_reg>(common::armgen::D16 + reg_index + i);
                    offset_from_state = offsetof(core_state, fprs_[32]) + sizeof(std::uint64_t) * (reg_index + i);

                    double_simd_infos_[reg_index + i].scratch_ = true;
                }

                if ((use_flags & FLOAT_MARKER_USE_WRITE) && !double_simd_infos_[reg_index + i].dirty_) {
                    double_simd_infos_[reg_index + i].dirty_ = true;
                }
            }

            if (base_load != common::armgen::INVALID_REG) {
                big_block_->VLDR(base_load, CORE_STATE_REG, offset_from_state);
            }
        }
    }

    void float_marker::sync_state(common::armgen::arm_reg state, const bool flush) {
        for (std::uint32_t i = 0; i < VFP_REG_COUNT; i++) {
            if (single_infos_[i].dirty_) {
                big_block_->VSTR(static_cast<common::armgen::arm_reg>(common::armgen::S0 + i), state,
                    offsetof(core_state, fprs_[0]) + sizeof(std::uint32_t) * i);

                if (flush)
                    single_infos_[i].dirty_ = false;
            } else {
                if (state != CORE_STATE_REG) {
                    big_block_->LDR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, fprs_[0]) + sizeof(std::uint32_t) * i);
                    big_block_->STR(ALWAYS_SCRATCH1, state, offsetof(core_state, fprs_[0]) + sizeof(std::uint32_t) * i);
                }
            }

            if (flush)
                single_infos_[i].scratch_ = false;
        }

        for (std::uint32_t i = 0; i < NEON_DOUBLE_REST_REG_COUNT; i++) {
            if (double_simd_infos_[i].dirty_) {
                big_block_->VSTR(static_cast<common::armgen::arm_reg>(common::armgen::D16 + i), state,
                    offsetof(core_state, fprs_[32]) + sizeof(std::uint64_t) * i);

                if (flush)
                    double_simd_infos_[i].dirty_ = false;
            } else {
                if (state != CORE_STATE_REG) {
                    big_block_->LDR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, fprs_[32]) + sizeof(std::uint64_t) * i);
                    big_block_->STR(ALWAYS_SCRATCH1, state, offsetof(core_state, fprs_[32]) + sizeof(std::uint64_t) * i);
                    big_block_->LDR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, fprs_[32]) + sizeof(std::uint64_t) * i + 4);
                    big_block_->STR(ALWAYS_SCRATCH1, state, offsetof(core_state, fprs_[32]) + sizeof(std::uint64_t) * i + 4);
                }
            }

            if (flush)
                double_simd_infos_[i].scratch_ = false;
        }
    }

    common::armgen::arm_reg decode_fp_reg(const bool db, const std::size_t base, const bool bit) {
        if (db) {
            return static_cast<common::armgen::arm_reg>(common::armgen::arm_reg::D0 + base + (bit ? 16 : 0));
        }

        return static_cast<common::armgen::arm_reg>(common::armgen::arm_reg::S0 + (base << 1) + (bit ? 1 : 0));
    }
}