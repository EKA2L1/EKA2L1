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

#include <cpu/12l1r/common.h>
#include <cpu/12l1r/arm_visitor.h>
#include <cpu/12l1r/block_gen.h>
#include <cpu/12l1r/visit_session.h>

#include <common/bytes.h>

namespace eka2l1::arm::r12l1 {
    bool arm_translate_visitor::arm_B(common::cc_flags cond, std::uint32_t imm24) {
        set_cond(cond);

        // Amount of move is in word unit, so shift two to left.
        const std::int32_t move_amount = static_cast<std::int32_t>(common::sign_extended<26, std::uint32_t>(imm24 << 2));
        const vaddress addr_to_jump = crr_block_->current_address() + move_amount + 8;

        emit_direct_link(addr_to_jump);

        return false;
    }

    bool arm_translate_visitor::arm_BL(common::cc_flags cond, std::uint32_t imm24) {
        set_cond(cond);

        // Amount of move is in word unit, so shift two to left.
        const std::int32_t move_amount = static_cast<std::int32_t>(common::sign_extended<26, std::uint32_t>(imm24 << 2));

        const vaddress addr_to_jump = crr_block_->current_address() + move_amount + 8;
        const vaddress next_instr_addr = crr_block_->current_address() + 4;

        common::armgen::arm_reg lr_reg_mapped = reg_supplier_.map(common::armgen::R14,
                ALLOCATE_FLAG_DIRTY);

        big_block_->MOVI2R(lr_reg_mapped, next_instr_addr);
        emit_direct_link(addr_to_jump);

        return false;
    }

    bool arm_translate_visitor::arm_BX(common::cc_flags cond, reg_index m) {
        set_cond(cond);

        common::armgen::arm_reg jump_reg_real = reg_index_to_gpr(m);
        common::armgen::arm_reg jump_reg_mapped = reg_supplier_.map(jump_reg_real, 0);

        big_block_->emit_pc_write_exchange(jump_reg_mapped);
        emit_return_to_dispatch();

        return false;
    }

    bool arm_translate_visitor::arm_BLX_reg(common::cc_flags cond, reg_index m) {
        set_cond(cond);

        common::armgen::arm_reg jump_reg_real = reg_index_to_gpr(m);
        common::armgen::arm_reg jump_reg_mapped = reg_supplier_.map(jump_reg_real, 0);

        const vaddress next_instr_addr = crr_block_->current_address() + 4;

        common::armgen::arm_reg lr_reg_mapped = reg_supplier_.map(common::armgen::R14,
                ALLOCATE_FLAG_DIRTY);

        big_block_->MOVI2R(lr_reg_mapped, next_instr_addr);
        big_block_->emit_pc_write_exchange(jump_reg_mapped);

        emit_return_to_dispatch();

        return false;
    }
}