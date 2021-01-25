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
#include <cpu/12l1r/thumb_visitor.h>
#include <cpu/12l1r/visit_session.h>

namespace eka2l1::arm::r12l1 {
    bool thumb_translate_visitor::thumb16_SXTH(reg_index m, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);

        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);

        big_block_->SXTH(dest_mapped, source_mapped);
        return true;
    }

    bool thumb_translate_visitor::thumb16_SXTB(reg_index m, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);

        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);

        big_block_->SXTB(dest_mapped, source_mapped);
        return true;
    }

    bool thumb_translate_visitor::thumb16_UXTH(reg_index m, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);

        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);

        big_block_->UXTH(dest_mapped, source_mapped);
        return true;
    }

    bool thumb_translate_visitor::thumb16_UXTB(reg_index m, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);

        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);

        big_block_->UXTB(dest_mapped, source_mapped);
        return true;
    }
}