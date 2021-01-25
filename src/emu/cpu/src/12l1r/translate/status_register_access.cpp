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
    bool arm_translate_visitor::arm_MRS(common::cc_flags cond, reg_index d) {
        if (!condition_passed(cond)) {
            return false;
        }

        // Selectively update our CPSR and move the result to mapped register.
        // This is to not reveal any host bits to the guest for security.
        emit_cpsr_update_sel();

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, 0);

        big_block_->MOV(dest_mapped, CPSR_REG);
        return true;
    }

    bool arm_translate_visitor::arm_MSR_imm(common::cc_flags cond, int mask, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        const bool update_nzcvq = mask & (1 << 3);
        const bool update_ge = mask & (1 << 2);
        const bool update_endian = mask & (1 << 1);

        if (update_endian) {
            LOG_WARN(CPU_12L1R, "Data endian flag not supported for updating!");
        }

        big_block_->_MSR(update_nzcvq, update_ge, common::armgen::operand2(imm8, static_cast<std::uint8_t>(rotate)));

        // Notify CPSR flags has changed
        cpsr_nzcvq_changed();
        cpsr_ge_changed();

        return true;
    }

    bool arm_translate_visitor::arm_MSR_reg(common::cc_flags cond, int mask, reg_index n) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg source_real = reg_index_to_gpr(n);
        common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);

        const bool update_nzcvq = mask & (1 << 3);
        const bool update_ge = mask & (1 << 2);
        const bool update_endian = mask & (1 << 1);

        if (update_endian) {
            LOG_WARN(CPU_12L1R, "Data endian flag not supported for updating!");
        }

        big_block_->_MSR(update_nzcvq, update_ge, source_mapped);

        // Notify CPSR flags has changed
        cpsr_nzcvq_changed();
        cpsr_ge_changed();

        return true;
    }
}