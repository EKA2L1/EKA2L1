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
}