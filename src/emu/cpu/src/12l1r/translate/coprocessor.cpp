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
#include <cpu/12l1r/core_state.h>
#include <cpu/12l1r/thumb_visitor.h>
#include <cpu/12l1r/visit_session.h>
#include <common/log.h>

namespace eka2l1::arm::r12l1 {
    bool arm_translate_visitor::arm_MCR(common::cc_flags cond, std::size_t opc1, int cr_n, reg_index t, std::size_t coproc_no,
                 std::size_t opc2, int cr_m) {
        if (!condition_passed(cond)) {
            return false;
        }

        if (coproc_no == 15) {
            if ((cr_n == 13) && (cr_m == 0) && (opc1 == 0) && (opc2 == 2)) {
                common::armgen::arm_reg source_real = reg_index_to_gpr(t);
                common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);

                big_block_->STR(source_mapped, CORE_STATE_REG, offsetof(core_state, wrwr_));
            } else {
                LOG_WARN(CPU_12L1R, "Emulator only handle access to WRWR in coprocessor, but this load requires something else!");
            }
        } else {
            LOG_WARN(CPU_12L1R, "Unhandled coprocessor {} for MCR instruction!", coproc_no);
        }

        return true;
    }

    bool arm_translate_visitor::arm_MRC(common::cc_flags cond, std::size_t opc1, int cr_n, reg_index t, std::size_t coproc_no,
                 size_t opc2, int cr_m) {
        if (!condition_passed(cond)) {
            return false;
        }

        if (coproc_no == 15) {
            if ((cr_n == 13) && (cr_m == 0) && (opc1 == 0) && (opc2 == 2)) {
                common::armgen::arm_reg dest_real = reg_index_to_gpr(t);
                common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

                big_block_->LDR(dest_mapped, CORE_STATE_REG, offsetof(core_state, wrwr_));
            } else {
                LOG_WARN(CPU_12L1R, "Emulator only handle access to WRWR in coprocessor, but this store requires something else!");
            }
        } else {
            LOG_WARN(CPU_12L1R, "Unhandled coprocessor {} for MRC instruction!", coproc_no);
        }

        return true;
    }
}