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
#include <cpu/12l1r/thumb_visitor.h>
#include <cpu/12l1r/block_gen.h>
#include <cpu/12l1r/visit_session.h>

namespace eka2l1::arm::r12l1 {
    bool arm_translate_visitor::arm_MUL(common::cc_flags cond, bool S, reg_index d, reg_index m, reg_index n) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);
        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (S) {
            big_block_->MULS(dest_mapped, op1_mapped, op2_mapped);
            cpsr_nzcvq_changed();
        } else {
            big_block_->MUL(dest_mapped, op1_mapped, op2_mapped);
        }

        return true;
    }

    bool arm_translate_visitor::arm_MLA(common::cc_flags cond, bool S, reg_index d, reg_index a, reg_index m, reg_index n) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);
        common::armgen::arm_reg add_real = reg_index_to_gpr(a);

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);
        const common::armgen::arm_reg add_mapped = reg_supplier_.map(add_real, 0);
        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (S) {
            big_block_->MLAS(dest_mapped, op1_mapped, op2_mapped, add_mapped);
            cpsr_nzcvq_changed();
        } else {
            big_block_->MLA(dest_mapped, op1_mapped, op2_mapped, add_mapped);
        }

        return true;
    }

    bool arm_translate_visitor::arm_UMLAL(common::cc_flags cond, bool S, reg_index d_hi, reg_index d_lo, reg_index m, reg_index n) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_hi_real = reg_index_to_gpr(d_hi);
        common::armgen::arm_reg dest_lo_real = reg_index_to_gpr(d_lo);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);
        const common::armgen::arm_reg dest_hi_mapped = reg_supplier_.map(dest_hi_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg dest_lo_mapped = reg_supplier_.map(dest_lo_real, ALLOCATE_FLAG_DIRTY);

        if (S) {
            big_block_->UMLALS(dest_lo_mapped, dest_hi_mapped, op1_mapped, op2_mapped);
            cpsr_nzcvq_changed();
        } else {
            big_block_->UMLAL(dest_lo_mapped, dest_hi_mapped, op1_mapped, op2_mapped);
        }

        return true;
    }

    bool arm_translate_visitor::arm_UMULL(common::cc_flags cond, bool S, reg_index d_hi, reg_index d_lo, reg_index m, reg_index n) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_hi_real = reg_index_to_gpr(d_hi);
        common::armgen::arm_reg dest_lo_real = reg_index_to_gpr(d_lo);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);
        const common::armgen::arm_reg dest_hi_mapped = reg_supplier_.map(dest_hi_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg dest_lo_mapped = reg_supplier_.map(dest_lo_real, ALLOCATE_FLAG_DIRTY);

        if (S) {
            big_block_->UMULLS(dest_lo_mapped, dest_hi_mapped, op1_mapped, op2_mapped);
            cpsr_nzcvq_changed();
        } else {
            big_block_->UMULL(dest_lo_mapped, dest_hi_mapped, op1_mapped, op2_mapped);
        }

        return true;
    }

    bool arm_translate_visitor::arm_SMULL(common::cc_flags cond, bool S, reg_index d_hi, reg_index d_lo, reg_index m, reg_index n) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_hi_real = reg_index_to_gpr(d_hi);
        common::armgen::arm_reg dest_lo_real = reg_index_to_gpr(d_lo);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);
        const common::armgen::arm_reg dest_hi_mapped = reg_supplier_.map(dest_hi_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg dest_lo_mapped = reg_supplier_.map(dest_lo_real, ALLOCATE_FLAG_DIRTY);

        if (S) {
            big_block_->SMULLS(dest_lo_mapped, dest_hi_mapped, op1_mapped, op2_mapped);
            cpsr_nzcvq_changed();
        } else {
            big_block_->SMULL(dest_lo_mapped, dest_hi_mapped, op1_mapped, op2_mapped);
        }

        return true;
    }

    bool thumb_translate_visitor::thumb16_MUL_reg(reg_index n, reg_index d_m) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_m);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(n);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
            ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);

        big_block_->MULS(dest_and_op1_mapped, dest_and_op1_mapped, op2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }
}