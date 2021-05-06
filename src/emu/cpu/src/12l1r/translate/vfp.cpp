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

#include <cpu/12l1r/arm_visitor.h>
#include <cpu/12l1r/block_gen.h>
#include <cpu/12l1r/visit_session.h>

namespace eka2l1::arm::r12l1 {
    bool arm_translate_visitor::vfp_VMOV_reg(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source, true);
        float_marker_.use(dest, false);

        big_block_->VMOV(dest, source);
        return true;
    }

    bool arm_translate_visitor::vfp_VMOV_u32_f32(common::cc_flags cond, const std::size_t Vn, reg_index t, bool N) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(false, Vn, N);
        common::armgen::arm_reg source = reg_index_to_gpr(t);

        float_marker_.use(dest, false);
        source = reg_supplier_.map(source, 0);

        big_block_->VMOV(dest, source);
        return true;
    }

    bool arm_translate_visitor::vfp_VMOV_f32_u32(common::cc_flags cond, const std::size_t Vn, reg_index t, bool N) {
        if (!condition_passed(cond)) {
            return false;
        }

        assert(t != 15);

        common::armgen::arm_reg dest = reg_index_to_gpr(t);
        common::armgen::arm_reg source = decode_fp_reg(false, Vn, N);

        float_marker_.use(source, true);
        dest = reg_supplier_.map(dest, ALLOCATE_FLAG_DIRTY);

        big_block_->VMOV(dest, source);
        return true;
    }

    bool arm_translate_visitor::vfp_VMOV_f64_2u32(common::cc_flags cond, reg_index t2, reg_index t, bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg source = decode_fp_reg(true, Vm, M);
        common::armgen::arm_reg first = reg_index_to_gpr(t);
        common::armgen::arm_reg second = reg_index_to_gpr(t2);

        first = reg_supplier_.map(first, ALLOCATE_FLAG_DIRTY);
        second = reg_supplier_.map(second, ALLOCATE_FLAG_DIRTY);

        float_marker_.use(source, true);

        big_block_->VMOV(first, second, source);
        return true;
    }

    bool arm_translate_visitor::vfp_VMOV_2u32_f64(common::cc_flags cond, reg_index t2, reg_index t, bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        if ((t == 15) || (t2 == 15)) {
            LOG_ERROR(CPU_12L1R, "Unpredictable VMOV.2u32.f64!");
            return true;
        }

        common::armgen::arm_reg dest = decode_fp_reg(true, Vm, M);
        common::armgen::arm_reg first = reg_index_to_gpr(t);
        common::armgen::arm_reg second = reg_index_to_gpr(t2);

        first = reg_supplier_.map(first, 0);
        second = reg_supplier_.map(second, 0);

        float_marker_.use(dest, false);

        big_block_->VMOV(dest, first, second);
        return true;
    }

    bool arm_translate_visitor::vfp_VCMP(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool E, bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg rhs = decode_fp_reg(sz, Vm, M);

        float_marker_.use(lhs, true);
        float_marker_.use(rhs, true);

        if (E) {
            big_block_->VCMPE(lhs, rhs);
        } else {
            big_block_->VCMP(lhs, rhs);
        }

        fpscr_changed();
        return true;
    }

    bool arm_translate_visitor::vfp_VCVT_to_u32(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool round_towards_zero,
        bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(false, Vd, D);
        common::armgen::arm_reg source = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source, true);
        float_marker_.use(dest, false);

        // To int flags, no sign
        big_block_->VCVT(dest, source, TO_INT | (round_towards_zero ? ROUND_TO_ZERO : 0));
        return true;
    }

    bool arm_translate_visitor::vfp_VCVT_to_s32(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool round_towards_zero,
        bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(false, Vd, D);
        common::armgen::arm_reg source = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source, true);
        float_marker_.use(dest, false);

        // To int flags, no sign
        big_block_->VCVT(dest, source, TO_INT | IS_SIGNED | (round_towards_zero ? ROUND_TO_ZERO : 0));
        return true;
    }

    bool arm_translate_visitor::vfp_VCVT_from_int(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool is_signed,
        bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source = decode_fp_reg(false, Vm, M);

        float_marker_.use(source, true);
        float_marker_.use(dest, false);

        int flags = (is_signed ? IS_SIGNED : 0);
        if (sz) {
            flags |= TO_INT;
        }

        // To int flags, no sign
        big_block_->VCVT(dest, source, flags);
        return true;
    }

    bool arm_translate_visitor::vfp_VCVT_f_to_f(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(!sz, Vd, D);
        common::armgen::arm_reg source = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source, true);
        float_marker_.use(dest, false);

        big_block_->VCVT(dest, source, 0);
        return true;
    }

    bool arm_translate_visitor::vfp_VADD(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz,
        bool N, bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source1 = decode_fp_reg(sz, Vn, N);
        common::armgen::arm_reg source2 = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source1, true);
        float_marker_.use(source2, true);
        float_marker_.use(dest, false);

        big_block_->VADD(dest, source1, source2);
        return true;
    }

    bool arm_translate_visitor::vfp_VSUB(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz,
        bool N, bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source1 = decode_fp_reg(sz, Vn, N);
        common::armgen::arm_reg source2 = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source1, true);
        float_marker_.use(source2, true);
        float_marker_.use(dest, false);

        big_block_->VSUB(dest, source1, source2);
        return true;
    }

    bool arm_translate_visitor::vfp_VMUL(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N,
        bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source1 = decode_fp_reg(sz, Vn, N);
        common::armgen::arm_reg source2 = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source1, true);
        float_marker_.use(source2, true);
        float_marker_.use(dest, false);

        big_block_->VMUL(dest, source1, source2);
        return true;
    }

    bool arm_translate_visitor::vfp_VMLA(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N,
        bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source1 = decode_fp_reg(sz, Vn, N);
        common::armgen::arm_reg source2 = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source1, true);
        float_marker_.use(source2, true);
        float_marker_.use(dest, false);

        big_block_->VMLA(dest, source1, source2);
        return true;
    }

    bool arm_translate_visitor::vfp_VMLS(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N,
        bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source1 = decode_fp_reg(sz, Vn, N);
        common::armgen::arm_reg source2 = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source1, true);
        float_marker_.use(source2, true);
        float_marker_.use(dest, false);

        big_block_->VMLS(dest, source1, source2);
        return true;
    }

    bool arm_translate_visitor::vfp_VNMUL(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source1 = decode_fp_reg(sz, Vn, N);
        common::armgen::arm_reg source2 = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source1, true);
        float_marker_.use(source2, true);
        float_marker_.use(dest, false);

        big_block_->VNMUL(dest, source1, source2);
        return true;
    }

    bool arm_translate_visitor::vfp_VNMLA(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source1 = decode_fp_reg(sz, Vn, N);
        common::armgen::arm_reg source2 = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source1, true);
        float_marker_.use(source2, true);
        float_marker_.use(dest, false);

        big_block_->VNMLA(dest, source1, source2);
        return true;
    }

    bool arm_translate_visitor::vfp_VNMLS(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source1 = decode_fp_reg(sz, Vn, N);
        common::armgen::arm_reg source2 = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source1, true);
        float_marker_.use(source2, true);
        float_marker_.use(dest, false);

        big_block_->VNMLS(dest, source1, source2);
        return true;
    }

    bool arm_translate_visitor::vfp_VDIV(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N,
        bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source1 = decode_fp_reg(sz, Vn, N);
        common::armgen::arm_reg source2 = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source1, true);
        float_marker_.use(source2, true);
        float_marker_.use(dest, false);

        big_block_->VDIV(dest, source1, source2);
        return true;
    }

    bool arm_translate_visitor::vfp_VNEG(common::cc_flags cond, bool D, std::size_t Vd, bool sz,
        bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source, true);
        float_marker_.use(dest, false);

        big_block_->VNEG(dest, source);
        return true;
    }

    bool arm_translate_visitor::vfp_VABS(common::cc_flags cond, bool D, std::size_t Vd, bool sz,
        bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source, true);
        float_marker_.use(dest, false);

        big_block_->VABS(dest, source);
        return true;
    }

    bool arm_translate_visitor::vfp_VSQRT(common::cc_flags cond, bool D, std::size_t Vd, bool sz,
        bool M, std::size_t Vm) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::arm_reg source = decode_fp_reg(sz, Vm, M);

        float_marker_.use(source, true);
        float_marker_.use(dest, false);

        big_block_->VSQRT(dest, source);
        return true;
    }

    bool arm_translate_visitor::vfp_VLDR(common::cc_flags cond, bool U, bool D, reg_index n, std::size_t Vd, bool sz, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg value_scr = reg_supplier_.scratch();
        common::armgen::arm_reg value_scr2 = common::armgen::INVALID_REG;
        if (sz) {
            value_scr2 = reg_supplier_.scratch();
        }
        common::armgen::arm_reg base = reg_index_to_gpr(n);
        common::armgen::arm_reg dest = decode_fp_reg(sz, Vd, D);
        common::armgen::operand2 adv(imm8 << 2);

        const bool result = emit_memory_access(value_scr, base, adv, sz ? 64 : 32, false, U, true, false, true,
            value_scr2, true);

        float_marker_.use(dest, false);

        if (sz) {
            big_block_->VMOV(dest, value_scr, value_scr2);
        } else {
            big_block_->VMOV(dest, value_scr);
        }

        reg_supplier_.done_scratching_this(value_scr);
        if (value_scr2 != common::armgen::INVALID_REG) {
            reg_supplier_.done_scratching_this(value_scr2);
        }

        return result;
    }

    bool arm_translate_visitor::vfp_VSTR(common::cc_flags cond, bool U, bool D, reg_index n, std::size_t Vd, bool sz, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg value_scr = reg_supplier_.scratch();
        common::armgen::arm_reg value_scr2 = common::armgen::INVALID_REG;
        if (sz) {
            value_scr2 = reg_supplier_.scratch();
        }

        common::armgen::arm_reg base = reg_index_to_gpr(n);
        common::armgen::arm_reg source = decode_fp_reg(sz, Vd, D);
        common::armgen::operand2 adv(imm8 << 2);

        // Load source values into these temp registers
        float_marker_.use(source, true);

        if (sz) {
            big_block_->VMOV(value_scr, value_scr2, source);
        } else {
            big_block_->VMOV(value_scr, source);
        }

        const bool result = emit_memory_access(value_scr, base, adv, sz ? 64 : 32, false, U, true, false, false,
            value_scr2, true);

        reg_supplier_.done_scratching_this(value_scr);
        if (value_scr2 != common::armgen::INVALID_REG) {
            reg_supplier_.done_scratching_this(value_scr2);
        }

        return result;
    }

    bool arm_translate_visitor::vfp_VPUSH(common::cc_flags cond, bool D, std::size_t Vd, bool sz, std::uint8_t imm8) {
        common::armgen::arm_reg start_push = decode_fp_reg(sz, Vd, D);
        int base_start = start_push - (sz ? common::armgen::D0 : common::armgen::R0);

        if (sz) {
            imm8 >>= 1;
        }

        reg_list list_to_push = 0;

        for (std::uint8_t i = 0; i < imm8; i++) {
            list_to_push |= (1 << (base_start + i));
            float_marker_.use(static_cast<common::armgen::arm_reg>(start_push + i), true);
        }

        return emit_memory_access_chain(common::armgen::R13, list_to_push, false, true, true, false, (sz ?
            ACCESS_CHAIN_REG_TYPE_D : ACCESS_CHAIN_REG_TYPE_S));
    }

    bool arm_translate_visitor::vfp_VPOP(common::cc_flags cond, bool D, std::size_t Vd, bool sz, std::uint8_t imm8) {
        common::armgen::arm_reg start_push = decode_fp_reg(sz, Vd, D);
        int base_start = start_push - (sz ? common::armgen::D0 : common::armgen::R0);

        if (sz) {
            imm8 >>= 1;
        }

        reg_list list_to_push = 0;

        for (std::uint8_t i = 0; i < imm8; i++) {
            list_to_push |= (1 << (base_start + i));
            float_marker_.use(static_cast<common::armgen::arm_reg>(start_push + i), false);
        }

        return emit_memory_access_chain(common::armgen::R13, list_to_push, true, false, true, true, (sz ?
            ACCESS_CHAIN_REG_TYPE_D : ACCESS_CHAIN_REG_TYPE_S));
    }

    bool arm_translate_visitor::vfp_VMRS(common::cc_flags cond, reg_index t) {
        if (!condition_passed(cond)) {
            return false;
        }

        if (t == 15) {
            big_block_->VMRS_APSR();
            cpsr_nzcvq_changed();
        } else {
            common::armgen::arm_reg dest = reg_index_to_gpr(t);
            dest = reg_supplier_.map(dest, ALLOCATE_FLAG_DIRTY);

            big_block_->VMRS(dest);
        }

        return true;
    }
}