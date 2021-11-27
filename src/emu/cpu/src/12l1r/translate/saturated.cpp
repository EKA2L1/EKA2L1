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
    bool arm_translate_visitor::arm_QADD(common::cc_flags cond, reg_index n, reg_index d, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        if ((n == 15) || (d == 15) || (m == 15)) {
            LOG_ERROR(CPU_12L1R, "Unpredicated QADD (one of the reg is PC!)");
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(n);

        op1_real = reg_supplier_.map(op1_real, 0);
        op2_real = reg_supplier_.map(op2_real, 0);
        dest_real = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        big_block_->QADD(dest_real, op1_real, op2_real);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_QSUB(common::cc_flags cond, reg_index n, reg_index d, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        if ((n == 15) || (d == 15) || (m == 15)) {
            LOG_ERROR(CPU_12L1R, "Unpredicated QADD (one of the reg is PC!)");
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(n);

        op1_real = reg_supplier_.map(op1_real, 0);
        op2_real = reg_supplier_.map(op2_real, 0);
        dest_real = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        big_block_->QSUB(dest_real, op1_real, op2_real);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_QDADD(common::cc_flags cond, reg_index n, reg_index d, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        if ((n == 15) || (d == 15) || (m == 15)) {
            LOG_ERROR(CPU_12L1R, "Unpredicated QADD (one of the reg is PC!)");
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(n);

        op1_real = reg_supplier_.map(op1_real, 0);
        op2_real = reg_supplier_.map(op2_real, 0);
        dest_real = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        big_block_->QDADD(dest_real, op1_real, op2_real);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_QDSUB(common::cc_flags cond, reg_index n, reg_index d, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        if ((n == 15) || (d == 15) || (m == 15)) {
            LOG_ERROR(CPU_12L1R, "Unpredicated QADD (one of the reg is PC!)");
            return false;
        }

        common::armgen::arm_reg op1_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(n);
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);

        dest_real = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        op1_real = reg_supplier_.map(op1_real, 0);
        op2_real = reg_supplier_.map(op2_real, 0);

        big_block_->QDSUB(dest_real, op1_real, op2_real);
        cpsr_nzcvq_changed();

        return true;
    }
}