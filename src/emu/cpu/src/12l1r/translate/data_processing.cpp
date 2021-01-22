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
#include <cpu/12l1r/thumb_visitor.h>
#include <cpu/12l1r/block_gen.h>
#include <cpu/12l1r/visit_session.h>

namespace eka2l1::arm::r12l1 {
    bool arm_translate_visitor::arm_MOV_imm(common::cc_flags cond, bool S, reg_index d, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);

        if (dest_real == common::armgen::R15) {
            LOG_TRACE(CPU_12L1R, "Undefined behaviour mov to r15, todo!");
            emit_undefined_instruction_handler();

            return false;
        }

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real,
                ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 imm_op(imm8, static_cast<std::uint8_t>(rotate));

        if (S) {
            big_block_->MOVS(dest_mapped, imm_op);
            cpsr_nzcvq_changed();
        } else {
            big_block_->MOV(dest_mapped, imm_op);
        }

        return true;
    }

    bool arm_translate_visitor::arm_MOV_reg(common::cc_flags cond, bool S, reg_index d, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);
        if (dest_real == common::armgen::R15) {
            emit_alu_jump(source_mapped);
            return false;
        }

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real,
                ALLOCATE_FLAG_DIRTY);

        if (source_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, expand_arm_shift(crr_block_->current_address() + 8,
                shift, imm5));

            return true;
        }

        common::armgen::operand2 imm_op(source_mapped, shift, imm5);

        if (S) {
            big_block_->MOVS(dest_mapped, imm_op);
            cpsr_nzcvq_changed();
        } else {
            big_block_->MOV(dest_mapped, imm_op);
        }

        return true;
    }

    bool arm_translate_visitor::arm_MOV_rsr(common::cc_flags cond, bool S, reg_index d, reg_index s,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);
        common::armgen::arm_reg shift_real = reg_index_to_gpr(s);

        if ((dest_real == common::armgen::R15) || (source_real == common::armgen::R15)) {
            LOG_WARN(CPU_12L1R, "MOV rsr to PC or from PC unimplemented!");
            return false;
        }

        common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);
        common::armgen::arm_reg shift_mapped = reg_supplier_.map(shift_real, 0);
        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(source_mapped, shift, shift_mapped);

        if (S) {
            big_block_->MOVS(dest_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->MOV(dest_mapped, op2);
        }

        return true;
    }

    bool arm_translate_visitor::arm_MVN_imm(common::cc_flags cond, bool S, reg_index d, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);

        if (dest_real == common::armgen::R15) {
            LOG_TRACE(CPU_12L1R, "Undefined behaviour mvn to r15, todo!");
            emit_undefined_instruction_handler();

            return false;
        }

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real,
            ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 imm_op(imm8, static_cast<std::uint8_t>(rotate));

        if (S) {
            big_block_->MVNS(dest_mapped, imm_op);
            cpsr_nzcvq_changed();
        } else {
            big_block_->MVN(dest_mapped, imm_op);
        }

        return true;
    }

    bool arm_translate_visitor::arm_MVN_reg(common::cc_flags cond, bool S, reg_index d, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);
        if (dest_real == common::armgen::R15) {
            emit_alu_jump(source_mapped);
            return false;
        }

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real,
            ALLOCATE_FLAG_DIRTY);

        if (source_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, ~expand_arm_shift(crr_block_->current_address() + 8,
                shift, imm5));

            return true;
        }

        common::armgen::operand2 imm_op(source_mapped, shift, imm5);

        if (S) {
            big_block_->MVNS(dest_mapped, imm_op);
            cpsr_nzcvq_changed();
        } else {
            big_block_->MVN(dest_mapped, imm_op);
        }

        return true;
    }

    bool arm_translate_visitor::arm_ADC_imm(common::cc_flags cond, bool S, reg_index n, reg_index d,
        int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            LOG_TRACE(CPU_12L1R, "Trying to add carry with an PC operand, weird behaviour that must note");

            assert(!S);
            big_block_->MOVI2R(dest_mapped, crr_block_->current_address() + 8 + expand_arm_imm(imm8, rotate));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->ADCS(dest_mapped, op1_mapped, op2);
                cpsr_nzcvq_changed();
            } else {
                big_block_->ADC(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ADC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d,
        std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg op1_mapped = ALWAYS_SCRATCH2;
        if (op1_real == common::armgen::R15) {
            big_block_->MOVI2R(op1_mapped, crr_block_->current_address() + 8);
        } else {
            op1_mapped = reg_supplier_.map(op1_real, 0);
        }

        if (op2_base_real == common::armgen::R15) {
            return emit_unimplemented_behaviour_handler();
        }

        common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->ADCS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->ADC(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ADD_imm(common::cc_flags cond, bool S, reg_index n, reg_index d,
        int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, crr_block_->current_address() + 8 + expand_arm_imm(imm8, rotate));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->ADDS(dest_mapped, op1_mapped, op2);
                cpsr_nzcvq_changed();
            } else {
                big_block_->ADD(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ADD_reg(common::cc_flags cond, bool S, reg_index n, reg_index d,
            std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg op1_mapped = ALWAYS_SCRATCH2;
        if (op1_real == common::armgen::R15) {
            big_block_->MOVI2R(op1_mapped, crr_block_->current_address() + 8);
        } else {
            op1_mapped = reg_supplier_.map(op1_real, 0);
        }

        common::armgen::arm_reg op2_base_mapped = ALWAYS_SCRATCH2;
        if (op2_base_real == common::armgen::R15) {
            big_block_->MOVI2R(op2_base_mapped, crr_block_->current_address() + 8);
        } else {
            op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        }

        common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->ADDS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->ADD(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ADD_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op_shift_real = reg_index_to_gpr(s);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm AND op that use PC!");
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        const common::armgen::arm_reg op_shift_mapped = reg_supplier_.map(op_shift_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, op_shift_mapped);

        if (S) {
            big_block_->ADDS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->ADD(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_SUB_imm(common::cc_flags cond, bool S, reg_index n, reg_index d,
        int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, crr_block_->current_address() + 8 - expand_arm_imm(imm8, rotate));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->SUBS(dest_mapped, op1_mapped, op2);
                cpsr_nzcvq_changed();
            } else {
                big_block_->SUB(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_SUB_reg(common::cc_flags cond, bool S, reg_index n, reg_index d,
            std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg op1_mapped = ALWAYS_SCRATCH2;
        if (op1_real == common::armgen::R15) {
            big_block_->MOVI2R(op1_mapped, crr_block_->current_address() + 8);
        } else {
            op1_mapped = reg_supplier_.map(op1_real, 0);
        }

        common::armgen::arm_reg op2_base_mapped = ALWAYS_SCRATCH2;
        if (op2_base_real == common::armgen::R15) {
            big_block_->MOVI2R(op2_base_mapped, crr_block_->current_address() + 8);
        } else {
            op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        }

        common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->SUBS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->SUB(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_SUB_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op_shift_real = reg_index_to_gpr(s);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm AND op that use PC!");
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        const common::armgen::arm_reg op_shift_mapped = reg_supplier_.map(op_shift_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, op_shift_mapped);

        if (S) {
            big_block_->SUBS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->SUB(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_SBC_imm(common::cc_flags cond, bool S, reg_index n, reg_index d,
        int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, crr_block_->current_address() + 8 - expand_arm_imm(imm8, rotate));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->SBCS(dest_mapped, op1_mapped, op2);
                cpsr_nzcvq_changed();
            } else {
                big_block_->SBC(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_SBC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d,
        std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg op1_mapped = ALWAYS_SCRATCH2;
        if (op1_real == common::armgen::R15) {
            big_block_->MOVI2R(op1_mapped, crr_block_->current_address() + 8);
        } else {
            op1_mapped = reg_supplier_.map(op1_real, 0);
        }

        common::armgen::arm_reg op2_base_mapped = ALWAYS_SCRATCH2;
        if (op2_base_real == common::armgen::R15) {
            big_block_->MOVI2R(op2_base_mapped, crr_block_->current_address() + 8);
        } else {
            op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        }

        common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->SBCS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->SBC(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_RSB_imm(common::cc_flags cond, bool S, reg_index n, reg_index d,
            int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, (-crr_block_->current_address() + 8) + expand_arm_imm(imm8, rotate));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->RSBS(dest_mapped, op1_mapped, op2);
                cpsr_nzcvq_changed();
            } else {
                big_block_->RSB(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_RSB_reg(common::cc_flags cond, bool S, reg_index n, reg_index d,
                                            std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg op1_mapped = ALWAYS_SCRATCH2;
        if (op1_real == common::armgen::R15) {
            big_block_->MOVI2R(op1_mapped, crr_block_->current_address() + 8);
        } else {
            op1_mapped = reg_supplier_.map(op1_real, 0);
        }

        common::armgen::arm_reg op2_base_mapped = ALWAYS_SCRATCH2;
        if (op2_base_real == common::armgen::R15) {
            big_block_->MOVI2R(op2_base_mapped, crr_block_->current_address() + 8);
        } else {
            op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        }

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->RSBS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->RSB(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_RSC_imm(common::cc_flags cond, bool S, reg_index n, reg_index d,
        int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, (-crr_block_->current_address() + 8) + expand_arm_imm(imm8, rotate));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->RSCS(dest_mapped, op1_mapped, op2);
                cpsr_nzcvq_changed();
            } else {
                big_block_->RSC(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_RSC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d,
        std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg op1_mapped = ALWAYS_SCRATCH2;
        if (op1_real == common::armgen::R15) {
            big_block_->MOVI2R(op1_mapped, crr_block_->current_address() + 8);
        } else {
            op1_mapped = reg_supplier_.map(op1_real, 0);
        }

        common::armgen::arm_reg op2_base_mapped = ALWAYS_SCRATCH2;
        if (op2_base_real == common::armgen::R15) {
            big_block_->MOVI2R(op2_base_mapped, crr_block_->current_address() + 8);
        } else {
            op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        }

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->RSCS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->RSC(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_BIC_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, ((crr_block_->current_address() + 8) & ~(expand_arm_imm(imm8, rotate))));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->BICS(dest_mapped, op1_mapped, op2);
                cpsr_nzcvq_changed();
            } else {
                big_block_->BIC(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_BIC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg op1_mapped = ALWAYS_SCRATCH2;
        if (op1_real == common::armgen::R15) {
            big_block_->MOVI2R(op1_mapped, crr_block_->current_address() + 8);
        } else {
            op1_mapped = reg_supplier_.map(op1_real, 0);
        }

        common::armgen::arm_reg op2_base_mapped = ALWAYS_SCRATCH2;
        if (op2_base_real == common::armgen::R15) {
            big_block_->MOVI2R(op2_base_mapped, crr_block_->current_address() + 8);
        } else {
            op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        }

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->BICS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->BIC(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_BIC_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op_shift_real = reg_index_to_gpr(s);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm BIC op that use PC!");
            return emit_unimplemented_behaviour_handler();
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        const common::armgen::arm_reg op_shift_mapped = reg_supplier_.map(op_shift_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, op_shift_mapped);

        if (S) {
            big_block_->BICS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->BIC(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ORR_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, ((crr_block_->current_address() + 8) | (expand_arm_imm(imm8, rotate))));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->ORRS(dest_mapped, op1_mapped, op2);
                cpsr_nzcvq_changed();
            } else {
                big_block_->ORR(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ORR_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg op1_mapped = ALWAYS_SCRATCH2;
        if (op1_real == common::armgen::R15) {
            big_block_->MOVI2R(op1_mapped, crr_block_->current_address() + 8);
        } else {
            op1_mapped = reg_supplier_.map(op1_real, 0);
        }

        common::armgen::arm_reg op2_base_mapped = ALWAYS_SCRATCH2;
        if (op2_base_real == common::armgen::R15) {
            big_block_->MOVI2R(op2_base_mapped, crr_block_->current_address() + 8);
        } else {
            op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        }

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->ORRS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->ORR(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_ORR_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op_shift_real = reg_index_to_gpr(s);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm ORR op that use PC!");
            return emit_unimplemented_behaviour_handler();
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        const common::armgen::arm_reg op_shift_mapped = reg_supplier_.map(op_shift_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, op_shift_mapped);

        if (S) {
            big_block_->ORRS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->ORR(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_EOR_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, ((crr_block_->current_address() + 8) ^ (expand_arm_imm(imm8, rotate))));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->EORS(dest_mapped, op1_mapped, op2);
                cpsr_nzcvq_changed();
            } else {
                big_block_->EOR(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_EOR_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg op1_mapped = ALWAYS_SCRATCH2;
        if (op1_real == common::armgen::R15) {
            big_block_->MOVI2R(op1_mapped, crr_block_->current_address() + 8);
        } else {
            op1_mapped = reg_supplier_.map(op1_real, 0);
        }

        common::armgen::arm_reg op2_base_mapped = ALWAYS_SCRATCH2;
        if (op2_base_real == common::armgen::R15) {
            big_block_->MOVI2R(op2_base_mapped, crr_block_->current_address() + 8);
        } else {
            op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        }

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->EORS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->EOR(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_EOR_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op_shift_real = reg_index_to_gpr(s);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm EOR op that use PC!");
            return emit_unimplemented_behaviour_handler();
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        const common::armgen::arm_reg op_shift_mapped = reg_supplier_.map(op_shift_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
            : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, op_shift_mapped);

        if (S) {
            big_block_->EORS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->EOR(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_CMP_imm(common::cc_flags cond, reg_index n, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::operand2 rhs(imm8, static_cast<std::uint8_t>(rotate));

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);

        big_block_->CMP(lhs_mapped, rhs);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_CMP_reg(common::cc_flags cond, reg_index n, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_base_mapped = reg_supplier_.map(rhs_base_real, 0);

        common::armgen::operand2 rhs(rhs_base_mapped, shift, imm5);

        big_block_->CMP(lhs_mapped, rhs);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_CMP_rsr(common::cc_flags cond, reg_index n, reg_index s, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_base_real = reg_index_to_gpr(m);
        common::armgen::arm_reg shift_real = reg_index_to_gpr(s);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_base_mapped = reg_supplier_.map(rhs_base_real, 0);
        common::armgen::arm_reg shift_mapped = reg_supplier_.map(shift_real, 0);

        common::armgen::operand2 rhs(rhs_base_mapped, shift, shift_mapped);

        big_block_->CMP(lhs_mapped, rhs);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_CMN_imm(common::cc_flags cond, reg_index n, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::operand2 rhs(imm8, static_cast<std::uint8_t>(rotate));

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);

        big_block_->CMN(lhs_mapped, rhs);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_CMN_reg(common::cc_flags cond, reg_index n, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_base_mapped = reg_supplier_.map(rhs_base_real, 0);

        common::armgen::operand2 rhs(rhs_base_mapped, shift, imm5);

        big_block_->CMN(lhs_mapped, rhs);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_TST_imm(common::cc_flags cond, reg_index n, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::operand2 rhs(imm8, static_cast<std::uint8_t>(rotate));

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);

        big_block_->TST(lhs_mapped, rhs);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_TST_reg(common::cc_flags cond, reg_index n, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_base_mapped = reg_supplier_.map(rhs_base_real, 0);

        common::armgen::operand2 rhs(rhs_base_mapped, shift, imm5);

        big_block_->TST(lhs_mapped, rhs);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_TST_rsr(common::cc_flags cond, reg_index n, reg_index s, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_base_real = reg_index_to_gpr(m);
        common::armgen::arm_reg shift_real = reg_index_to_gpr(s);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_base_mapped = reg_supplier_.map(rhs_base_real, 0);
        common::armgen::arm_reg shift_mapped = reg_supplier_.map(shift_real, 0);

        common::armgen::operand2 rhs(rhs_base_mapped, shift, shift_mapped);

        big_block_->TST(lhs_mapped, rhs);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_TEQ_imm(common::cc_flags cond, reg_index n, int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::operand2 rhs(imm8, static_cast<std::uint8_t>(rotate));

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);

        big_block_->TEQ(lhs_mapped, rhs);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_TEQ_reg(common::cc_flags cond, reg_index n, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_base_real = reg_index_to_gpr(m);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_base_mapped = reg_supplier_.map(rhs_base_real, 0);

        common::armgen::operand2 rhs(rhs_base_mapped, shift, imm5);

        big_block_->TEQ(lhs_mapped, rhs);
        cpsr_nzcvq_changed();

        return true;
    }

    bool arm_translate_visitor::arm_AND_imm(common::cc_flags cond, bool S, reg_index n, reg_index d,
        int rotate, std::uint8_t imm8) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);

        common::armgen::operand2 op2(imm8, static_cast<std::uint8_t>(rotate));

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        if (op1_real == common::armgen::R15) {
            assert(!S);
            big_block_->MOVI2R(dest_mapped, ((crr_block_->current_address() + 8) & (expand_arm_imm(imm8, rotate))));
        } else {
            const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

            if (S) {
                big_block_->ANDS(dest_mapped, op1_mapped, op2);
                cpsr_nzcvq_changed();
            } else {
                big_block_->AND(dest_mapped, op1_mapped, op2);
            }
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_AND_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm AND op that use PC!");
            return emit_unimplemented_behaviour_handler();
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, imm5);

        if (S) {
            big_block_->ANDS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->AND(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool arm_translate_visitor::arm_AND_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
        common::armgen::shift_type shift, reg_index m) {
        if (!condition_passed(cond)) {
            return false;
        }

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_base_real = reg_index_to_gpr(m);
        common::armgen::arm_reg op_shift_real = reg_index_to_gpr(s);

        if ((op1_real == common::armgen::R15) || (op2_base_real == common::armgen::R15)) {
            LOG_ERROR(CPU_12L1R, "Unsupported non-imm AND op that use PC!");
        }

        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_base_mapped = reg_supplier_.map(op2_base_real, 0);
        const common::armgen::arm_reg op_shift_mapped = reg_supplier_.map(op_shift_real, 0);

        const common::armgen::arm_reg dest_mapped = (dest_real == common::armgen::R15) ? ALWAYS_SCRATCH1
                : reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 op2(op2_base_mapped, shift, op_shift_mapped);

        if (S) {
            big_block_->ANDS(dest_mapped, op1_mapped, op2);
            cpsr_nzcvq_changed();
        } else {
            big_block_->AND(dest_mapped, op1_mapped, op2);
        }

        if (dest_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped);
            return false;
        }

        return true;
    }

    bool thumb_translate_visitor::thumb16_MOV_imm(reg_index d, std::uint8_t imm8) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        common::armgen::operand2 imm_op(imm8, 0);

        big_block_->MOVS(dest_mapped, imm_op);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_MOV_reg(bool d_hi, reg_index m, reg_index d_lo) {
        const reg_index d = d_hi ? (d_lo + 8) : d_lo;
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);
        if (dest_real == common::armgen::R15) {
            emit_alu_jump(source_mapped);
            return false;
        }

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real,
                ALLOCATE_FLAG_DIRTY);

        if (source_real == common::armgen::R15) {
            big_block_->MOVI2R(dest_mapped, crr_block_->current_address() + 4);
            return true;
        }

        big_block_->MOV(dest_mapped, source_mapped);

        return true;
    }

    bool thumb_translate_visitor::thumb16_MVN_reg(reg_index m, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source_real = reg_index_to_gpr(m);

        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        common::armgen::arm_reg source_mapped = reg_supplier_.map(source_real, 0);

        big_block_->MVNS(dest_mapped, source_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ADD_imm_t1(std::uint8_t imm3, reg_index n, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::operand2 op2(imm3, 0);

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

        big_block_->ADDS(dest_mapped, op1_mapped, op2);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ADD_imm_t2(reg_index d_n, std::uint8_t imm8) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::operand2 op2(imm8, 0);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
            ALLOCATE_FLAG_DIRTY);

        big_block_->ADDS(dest_and_op1_mapped, dest_and_op1_mapped, op2);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ADD_reg_t1(reg_index m, reg_index n, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg source1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg source2_real = reg_index_to_gpr(m);

        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        common::armgen::arm_reg source1_mapped = common::armgen::INVALID_REG;
        common::armgen::arm_reg source2_mapped = common::armgen::INVALID_REG;

        if (source1_real == common::armgen::R15) {
            if (source2_real == common::armgen::R15) {
                LOG_ERROR(CPU_12L1R, "Invalid add instruction!");
                return false;
            }

            source1_mapped = ALWAYS_SCRATCH1;
            source2_mapped = reg_supplier_.map(source2_real, 0);

            big_block_->MOVI2R(source1_mapped, crr_block_->current_address() + 4);
        } else {
            source1_mapped = reg_supplier_.map(source1_real, 0);

            // If source1 is not R15 then source2 maybe R15, no problem here.
            if (source2_real == common::armgen::R15) {
                source2_mapped = ALWAYS_SCRATCH1;
                big_block_->MOVI2R(source2_mapped, crr_block_->current_address() + 4);
            } else {
                source2_mapped = reg_supplier_.map(source2_real, 0);
            }
        }

        big_block_->ADDS(dest_mapped, source1_mapped, source2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ADD_reg_t2(bool d_n_hi, reg_index m, reg_index d_n_lo) {
        reg_index d_n = (d_n_hi) ? (d_n_lo + 8) : d_n_lo;

        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        if ((d_n == 15) && (m == 15)) {
            LOG_WARN(CPU_12L1R, "Unpredictable Thumb ADD");
            return false;
        }

        common::armgen::arm_reg dest_and_op1_mapped = common::armgen::INVALID_REG;
        common::armgen::arm_reg op2_mapped = common::armgen::INVALID_REG;

        if (op2_real == common::armgen::R15) {
            op2_mapped = ALWAYS_SCRATCH1;
            big_block_->MOVI2R(op2_mapped, crr_block_->current_address() + 4);
        } else {
            op2_mapped = reg_supplier_.map(op2_real, 0);
        }

        if (dest_and_op1_real == common::armgen::R15) {
            dest_and_op1_mapped = ALWAYS_SCRATCH1;
            big_block_->MOVI2R(dest_and_op1_mapped, crr_block_->current_address() + 4);
        } else {
            dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real, ALLOCATE_FLAG_DIRTY);
        }

        common::armgen::arm_reg dest_mapped_final = dest_and_op1_mapped;

        if (dest_and_op1_real == common::armgen::R15) {
            dest_mapped_final = ALWAYS_SCRATCH2;
        }

        big_block_->ADD(dest_mapped_final, dest_and_op1_mapped, op2_mapped);

        if (dest_and_op1_real == common::armgen::R15) {
            emit_alu_jump(dest_mapped_final);
            return false;
        }

        return true;
    }

    bool thumb_translate_visitor::thumb16_ADD_sp_t1(reg_index d, std::uint8_t imm8) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);

        common::armgen::arm_reg sp_mapped = reg_supplier_.map(common::armgen::R13, 0);
        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        big_block_->ADDI2R(dest_mapped, sp_mapped, imm8 << 2, ALWAYS_SCRATCH1);
        return true;
    }

    bool thumb_translate_visitor::thumb16_ADD_sp_t2(std::uint8_t imm7) {
        const common::armgen::arm_reg sp_mapped = reg_supplier_.map(common::armgen::R13,
            ALLOCATE_FLAG_DIRTY);

        big_block_->ADDI2R(sp_mapped, sp_mapped, imm7 << 2, ALWAYS_SCRATCH1);
        return true;
    }

    bool thumb_translate_visitor::thumb16_SUB_imm_t1(std::uint8_t imm3, reg_index n, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::operand2 op2(imm3, 0);

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

        big_block_->SUBS(dest_mapped, op1_mapped, op2);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_SUB_imm_t2(reg_index d_n, std::uint8_t imm8) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::operand2 op2(imm8, 0);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
            ALLOCATE_FLAG_DIRTY);

        big_block_->SUBS(dest_and_op1_mapped, dest_and_op1_mapped, op2);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_SUB_sp(std::uint8_t imm7) {
        const common::armgen::arm_reg sp_mapped = reg_supplier_.map(common::armgen::R13,
            ALLOCATE_FLAG_DIRTY);

        big_block_->SUBI2R(sp_mapped, sp_mapped, imm7 << 2, ALWAYS_SCRATCH1);
        return true;
    }

    bool thumb_translate_visitor::thumb16_SUB_reg(reg_index m, reg_index n, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);

        big_block_->SUBS(dest_mapped, op1_mapped, op2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_LSL_imm(std::uint8_t imm5, reg_index m, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(m);
        common::armgen::operand2 op2(imm5, 0);

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

        big_block_->LSLS(dest_mapped, op1_mapped, op2);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_LSR_imm(std::uint8_t imm5, reg_index m, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(m);
        common::armgen::operand2 op2(imm5, 0);

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

        big_block_->LSRS(dest_mapped, op1_mapped, op2);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ASR_imm(std::uint8_t imm5, reg_index m, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(m);
        common::armgen::operand2 op2(imm5, 0);

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

        big_block_->ASRS(dest_mapped, op1_mapped, op2);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_CMP_imm(reg_index n, std::uint8_t imm8) {
        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::operand2 rhs(imm8, 0);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        big_block_->CMP(lhs_mapped, rhs);

        cpsr_nzcvq_changed();
        return true;
    }

    bool thumb_translate_visitor::thumb16_CMP_reg_t1(reg_index m, reg_index n) {
        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_real = reg_index_to_gpr(m);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_mapped = reg_supplier_.map(rhs_real, 0);

        big_block_->CMP(lhs_mapped, rhs_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_CMP_reg_t2(bool n_hi, reg_index m, reg_index n_lo) {
        reg_index n = (n_hi) ? (n_lo + 8) : n_lo;

        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_real = reg_index_to_gpr(m);

        if ((n < 8) && (m < 8)) {
            LOG_WARN(CPU_12L1R, "Unpredictable Thumb CMP");
            return false;
        }

        if ((n == 15) || (m == 15)) {
            LOG_WARN(CPU_12L1R, "Unpredictable Thumb CMP");
            return false;
        }

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_mapped = reg_supplier_.map(rhs_real, 0);

        big_block_->CMP(lhs_mapped, rhs_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_CMN_reg(reg_index m, reg_index n) {
        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_real = reg_index_to_gpr(m);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_mapped = reg_supplier_.map(rhs_real, 0);

        big_block_->CMN(lhs_mapped, rhs_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_AND_reg(reg_index m, reg_index d_n) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
            ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);

        big_block_->ANDS(dest_and_op1_mapped, dest_and_op1_mapped, op2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ORR_reg(reg_index m, reg_index d_n) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
                                                                              ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);

        big_block_->ORRS(dest_and_op1_mapped, dest_and_op1_mapped, op2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ADR(reg_index d, std::uint8_t imm8) {
        const std::uint32_t data_addr = crr_block_->current_aligned_address() +
            (imm8 << 2) + 4;

        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);

        big_block_->MOVI2R(dest_mapped, data_addr);

        return true;
    }

    bool thumb_translate_visitor::thumb16_RSB_imm(reg_index n, reg_index d) {
        common::armgen::arm_reg dest_real = reg_index_to_gpr(d);
        common::armgen::arm_reg op1_real = reg_index_to_gpr(n);
        common::armgen::operand2 op2(0);

        const common::armgen::arm_reg dest_mapped = reg_supplier_.map(dest_real, ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op1_mapped = reg_supplier_.map(op1_real, 0);

        big_block_->RSBS(dest_mapped, op1_mapped, op2);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_BIC_reg(reg_index m, reg_index d_n) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
            ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);

        big_block_->BICS(dest_and_op1_mapped, dest_and_op1_mapped, op2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_EOR_reg(reg_index m, reg_index d_n) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
            ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);

        big_block_->EORS(dest_and_op1_mapped, dest_and_op1_mapped, op2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ASR_reg(reg_index m, reg_index d_n) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
            ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);

        big_block_->ASRS(dest_and_op1_mapped, dest_and_op1_mapped, op2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_LSR_reg(reg_index m, reg_index d_n) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
                                                                              ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);

        big_block_->LSRS(dest_and_op1_mapped, dest_and_op1_mapped, op2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_LSL_reg(reg_index m, reg_index d_n) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
                                                                              ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);

        big_block_->LSLS(dest_and_op1_mapped, dest_and_op1_mapped, op2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_TST_reg(reg_index m, reg_index n) {
        common::armgen::arm_reg lhs_real = reg_index_to_gpr(n);
        common::armgen::arm_reg rhs_real = reg_index_to_gpr(m);

        common::armgen::arm_reg lhs_mapped = reg_supplier_.map(lhs_real, 0);
        common::armgen::arm_reg rhs_mapped = reg_supplier_.map(rhs_real, 0);

        big_block_->TST(lhs_mapped, rhs_mapped);
        cpsr_nzcvq_changed();

        return true;
    }

    bool thumb_translate_visitor::thumb16_ADC_reg(reg_index m, reg_index d_n) {
        common::armgen::arm_reg dest_and_op1_real = reg_index_to_gpr(d_n);
        common::armgen::arm_reg op2_real = reg_index_to_gpr(m);

        const common::armgen::arm_reg dest_and_op1_mapped = reg_supplier_.map(dest_and_op1_real,
                                                                              ALLOCATE_FLAG_DIRTY);
        const common::armgen::arm_reg op2_mapped = reg_supplier_.map(op2_real, 0);

        big_block_->ADCS(dest_and_op1_mapped, dest_and_op1_mapped, op2_mapped);
        cpsr_nzcvq_changed();

        return true;
    }
}