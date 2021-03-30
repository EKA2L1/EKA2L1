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

#include <cpu/12l1r/common.h>
#include <cpu/12l1r/reg_loc.h>

#include <cstdint>

namespace eka2l1::arm::r12l1 {
    class dashixiong_block;

    enum reg_scratch_type {
        REG_SCRATCH_TYPE_GPR = 0,
        REG_SCRATCH_TYPE_FPR = 1
    };

    enum allocate_flags {
        ALLOCATE_FLAG_SCRATCH = 1 << 0,
        ALLOCATE_FLAG_DIRTY = 1 << 1
    };

    class reg_cache {
    private:
        static constexpr std::uint32_t HOST_GPRS_LENGTH = 16;
        static constexpr std::uint32_t HOST_FPRS_LENGTH = 32;

        static constexpr std::uint32_t GUEST_GPRS_LENGTH = 16;
        static constexpr std::uint32_t GUEST_FPRS_LENGTH = 32;

        host_register_info host_gpr_infos_[HOST_GPRS_LENGTH];
        host_register_info host_fpr_infos_[HOST_FPRS_LENGTH];

        guest_register_info guest_gpr_infos_[GUEST_GPRS_LENGTH];
        guest_register_info guest_fpr_infos_[GUEST_FPRS_LENGTH];

        std::uint32_t time_;
        dashixiong_block *big_block_;

    protected:
        bool load_gpr_to_host(common::armgen::arm_reg dest_reg, common::armgen::arm_reg source_guest_reg);

        void flush_gpr(const common::armgen::arm_reg mee);
        void flush_host_gpr(const common::armgen::arm_reg mee);

        common::armgen::arm_reg allocate_or_spill(reg_scratch_type type, const std::uint32_t flags);

    public:
        explicit reg_cache(dashixiong_block *bblock);

        /**
         * @brief       Map a guest register to host register.
         * 
         * @param       mee The guest register to map to.
         * @returns     INVALID_REG on failure, mapped register on success.
         */
        common::armgen::arm_reg map(const common::armgen::arm_reg mee, const std::uint32_t allocate_flags);

        /**
         * @brief   Provide a scratch register for temporary purpose.
         * 
         * The recompiler has always two scratch registers available. However, if they are not sufficient enough
         * for the use, this function can contact the inside of the cache to provide you some other registers.
         * 
         * @param   type        Type of scratch register (from GPR or from FPR)
         * @returns INVALID_REG on failure, a scratch register on success.
         */
        common::armgen::arm_reg scratch(reg_scratch_type type);

        void set_imm(const common::armgen::arm_reg mee, const std::uint32_t imm);

        void flush(const common::armgen::arm_reg guest_mee);
        void flush_all();

        void done_scratching(reg_scratch_type type);
        void done_scratching_this(common::armgen::arm_reg mee);
        void force_scratch(common::armgen::arm_reg mee);

        void flush_host_reg(const common::armgen::arm_reg host_reg);
        void flush_host_regs_for_host_call();

        void spill_lock(const common::armgen::arm_reg guest_reg);
        void spill_lock_all(reg_scratch_type type);

        void release_spill_lock(const common::armgen::arm_reg guest_reg);
        void release_spill_lock_all(reg_scratch_type type);

        void copy_state(common::armgen::arm_reg state_reg);
    };
}