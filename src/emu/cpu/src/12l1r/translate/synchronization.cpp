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
    bool arm_translate_visitor::arm_STREX(common::cc_flags cond, reg_index n, reg_index d, reg_index t) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg status_real = reg_index_to_gpr(d);
        common::armgen::arm_reg base_real = reg_index_to_gpr(n);
        common::armgen::arm_reg source_real = reg_index_to_gpr(t);

        return emit_memory_write_exclusive(status_real, source_real, base_real, 32);
    }

    bool arm_translate_visitor::arm_LDREX(common::cc_flags cond, reg_index n, reg_index t) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg base_real = reg_index_to_gpr(n);
        common::armgen::arm_reg dest_real = reg_index_to_gpr(t);

        return emit_memory_read_exclusive(dest_real, base_real, 32);
    }
}