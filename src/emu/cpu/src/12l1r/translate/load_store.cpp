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

#include <cpu/12l1r/arm_visitor.h>
#include <cpu/12l1r/thumb_visitor.h>
#include <cpu/12l1r/visit_session.h>
#include <cpu/12l1r/block_cache.h>
#include <cpu/12l1r/block_gen.h>

#include <common/algorithm.h>

namespace eka2l1::arm::r12l1 {
    bool arm_translate_visitor::arm_LDR_lit(common::cc_flags cond, bool U, reg_index t, std::uint16_t imm12) {
        set_cond(cond);

        // Aligning down
        const std::uint32_t base = common::align(crr_block_->current_address(), 4, 0) + 8;
        const std::uint32_t data_addr = (U ? (base + imm12) : (base - imm12));

        common::armgen::operand2 adv(0);
        common::armgen::arm_reg dest_real = reg_index_to_gpr(t);

        common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ?
                reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR) : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        common::armgen::arm_reg base_mapped = reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR);

        big_block_->MOVI2R(base_mapped, data_addr);
        if (!emit_memory_access(dest_mapped, base_mapped, adv, 32, false, true, true, false, true)) {
            LOG_ERROR(CPU_12L1R, "Some error occured during memory access emit!");
            return false;
        }

        if (dest_real == common::armgen::R15) {
            // Write the result
            big_block_->emit_pc_write_exchange(dest_mapped);
            emit_return_to_dispatch();

            reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);

            return false;
        }

        reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);
        return true;
    }

    bool arm_translate_visitor::arm_LDR_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index d, std::uint16_t imm12) {
        set_cond(cond);

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg base_real = reg_index_to_gpr(n);

        common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR)
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::arm_reg base_mapped = reg_supplier_.map(base_real, W ? ALLOCATE_FLAG_DIRTY : 0);
        common::armgen::operand2 adv(imm12);

        if (!emit_memory_access(dest_mapped, base_mapped, adv, 32, false, U, P, W, true)) {
            LOG_ERROR(CPU_12L1R, "Some error occured during memory access emit!");
            return false;
        }

        if (dest_real == common::armgen::R15) {
            // Write the result
            big_block_->emit_pc_write_exchange(dest_mapped);
            reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);

            emit_return_to_dispatch();
            return false;
        }

        reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);
        return true;
    }

    bool arm_translate_visitor::arm_LDR_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index d, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        set_cond(cond);

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg base_real = reg_index_to_gpr(n);
        common::armgen::arm_reg offset_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR)
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        common::armgen::arm_reg base_mapped = reg_supplier_.map(base_real, W ? ALLOCATE_FLAG_DIRTY : 0);
        common::armgen::arm_reg offset_base_mapped = reg_supplier_.map(offset_base_real, 0);

        common::armgen::operand2 adv(offset_base_mapped, shift, imm5);

        if (!emit_memory_access(dest_mapped, base_mapped, adv, 32, false, U, P, W, true)) {
            LOG_ERROR(CPU_12L1R, "Some error occured during memory access emit!");
            return false;
        }

        if (dest_real == common::armgen::R15) {
            // Write the result
            big_block_->emit_pc_write_exchange(dest_mapped);
            reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);

            emit_return_to_dispatch();
            return false;
        }

        reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);
        return true;
    }

    bool arm_translate_visitor::arm_STR_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint16_t imm12) {
        set_cond(cond);

        common::armgen::arm_reg dest_real = reg_index_to_gpr(t);
        common::armgen::arm_reg base_real = reg_index_to_gpr(n);

        common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR)
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::arm_reg base_mapped = reg_supplier_.map(base_real, W ? ALLOCATE_FLAG_DIRTY : 0);
        common::armgen::operand2 adv(imm12);

        if (!emit_memory_access(dest_mapped, base_mapped, adv, 32, false, U, P, W, false)) {
            LOG_ERROR(CPU_12L1R, "Some error occured during memory access emit!");
            return false;
        }

        if (dest_real == common::armgen::R15) {
            // Write the result
            big_block_->emit_pc_write_exchange(dest_mapped);
            reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);

            emit_return_to_dispatch();
            return false;
        }

        reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);
        return true;
    }

    bool arm_translate_visitor::arm_STR_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        set_cond(cond);

        common::armgen::arm_reg dest_real = reg_index_to_gpr(t);
        common::armgen::arm_reg base_real = reg_index_to_gpr(n);
        common::armgen::arm_reg offset_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR)
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::arm_reg base_mapped = reg_supplier_.map(base_real, W ? ALLOCATE_FLAG_DIRTY : 0);
        common::armgen::arm_reg offset_base_mapped = reg_supplier_.map(offset_base_real, 0);

        common::armgen::operand2 adv(offset_base_mapped, shift, imm5);

        if (!emit_memory_access(dest_mapped, base_mapped, adv, 32, false, U, P, W, false)) {
            LOG_ERROR(CPU_12L1R, "Some error occured during memory access emit!");
            return false;
        }

        if (dest_real == common::armgen::R15) {
            // Write the result
            big_block_->emit_pc_write_exchange(dest_mapped);
            reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);

            emit_return_to_dispatch();
            return false;
        }

        reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);
        return true;
    }

    bool arm_translate_visitor::arm_LDM(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        set_cond(cond, true);
        return emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, true, false, W, true);
    }

    bool arm_translate_visitor::arm_LDMDA(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        set_cond(cond, true);
        return emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, false, false, W, true);
    }

    bool arm_translate_visitor::arm_LDMDB(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        set_cond(cond, true);
        return emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, false, true, W, true);
    }

    bool arm_translate_visitor::arm_LDMIB(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        set_cond(cond, true);
        return emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, true, true, W, true);
    }

    bool arm_translate_visitor::arm_STM(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        set_cond(cond, true);
        return emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, true, false, W, false);
    }

    bool arm_translate_visitor::arm_STMDA(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        set_cond(cond, true);
        return emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, false, false, W, false);
    }
    
    bool arm_translate_visitor::arm_STMDB(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        set_cond(cond, true);
        return emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, false, true, W, false);
    }

    bool arm_translate_visitor::arm_STMIB(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        set_cond(cond, true);
        return emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, true, true, W, false);
    }

    bool thumb_translate_visitor::thumb16_PUSH(bool m, reg_list reg_list) {
        if (m) {
            reg_list |= 1 << 14;
        }

        return emit_memory_access_chain(common::armgen::R13, reg_list, false, true, true, false);
    }

    bool thumb_translate_visitor::thumb16_POP(bool p, reg_list reg_list) {
        if (p) {
            reg_list |= 1 << 15;
        }

        return emit_memory_access_chain(common::armgen::R13, reg_list, true, false, true, true);
    }
}