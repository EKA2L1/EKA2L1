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
    bool arm_translate_visitor::arm_BL(common::cc_flags cond, std::uint32_t imm24) {
        session_->set_cond(cond);

        // Amount of move is in word unit, so shift two to left.
        const std::int32_t move_amount = static_cast<std::int32_t>(common::sign_extended<26, std::uint32_t>(imm24 << 2));

        const vaddress addr_to_jump = session_->crr_block_->current_address() + move_amount + 8;
        const vaddress next_instr_addr = session_->crr_block_->current_address() + 4;

        session_->crr_block_->link_type_ = TRANSLATED_BLOCK_LINK_KNOWN;
        session_->crr_block_->link_to_ = addr_to_jump;

        common::armgen::arm_reg lr_reg_mapped = session_->reg_supplier_.map(common::armgen::R14,
                ALLOCATE_FLAG_DIRTY);

        session_->big_block_->MOVI2R(lr_reg_mapped, next_instr_addr);
        return false;
    }
}