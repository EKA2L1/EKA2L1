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

#pragma once

#include <cpu/12l1r/common.h>
#include <cpu/12l1r/visit_session.h>
#include <common/armemitter.h>

namespace eka2l1::arm::r12l1 {
    class arm_translate_visitor: public visit_session {
    public:
        using instruction_return_type = bool;

        explicit arm_translate_visitor(dashixiong_block *bro, translated_block *crr);

        // Data processing
        bool arm_MOV_imm(common::cc_flags cond, bool S, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_MOV_reg(common::cc_flags cond, bool S, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_MOV_rsr(common::cc_flags cond, bool S, reg_index d, reg_index s, common::armgen::shift_type shift,
            reg_index m);
        bool arm_MVN_imm(common::cc_flags cond, bool S, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_MVN_reg(common::cc_flags cond, bool S, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_ADC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_ADD_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_ADD_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_ADD_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
            common::armgen::shift_type shift, reg_index m);
        bool arm_SUB_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_SUB_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_SUB_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
            common::armgen::shift_type shift, reg_index m);
        bool arm_SBC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_RSB_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_RSB_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_BIC_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_BIC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_BIC_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
            common::armgen::shift_type shift, reg_index m);
        bool arm_ORR_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_ORR_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_ORR_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
            common::armgen::shift_type shift, reg_index m);
        bool arm_EOR_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_EOR_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_EOR_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
            common::armgen::shift_type shift, reg_index m);
        bool arm_CMP_imm(common::cc_flags cond, reg_index n, int rotate, std::uint8_t imm8);
        bool arm_CMP_reg(common::cc_flags cond, reg_index n, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m);
        bool arm_CMP_rsr(common::cc_flags cond, reg_index n, reg_index s, common::armgen::shift_type shift, reg_index m);
        bool arm_CMN_imm(common::cc_flags cond, reg_index n, int rotate, std::uint8_t imm8);
        bool arm_CMN_reg(common::cc_flags cond, reg_index n, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m);
        bool arm_TST_imm(common::cc_flags cond, reg_index n, int rotate, std::uint8_t imm8);
        bool arm_TST_reg(common::cc_flags cond, reg_index n, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m);
        bool arm_TEQ_reg(common::cc_flags cond, reg_index n, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m);
        bool arm_AND_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_AND_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_AND_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
            common::armgen::shift_type shift, reg_index m);

        // Branch
        bool arm_B(common::cc_flags cond, std::uint32_t imm24);
        bool arm_BL(common::cc_flags cond, std::uint32_t imm24);
        bool arm_BX(common::cc_flags cond, reg_index m);
        bool arm_BLX_reg(common::cc_flags cond, reg_index m);

        // Load/store
        bool arm_LDM(common::cc_flags cond, bool W, reg_index n, reg_list list);
        bool arm_LDMDA(common::cc_flags cond, bool W, reg_index n, reg_list list);
        bool arm_LDMDB(common::cc_flags cond, bool W, reg_index n, reg_list list);
        bool arm_LDMIB(common::cc_flags cond, bool W, reg_index n, reg_list list);
        bool arm_STM(common::cc_flags cond, bool W, reg_index n, reg_list list);
        bool arm_STMDA(common::cc_flags cond, bool W, reg_index n, reg_list list);
        bool arm_STMDB(common::cc_flags cond, bool W, reg_index n, reg_list list);
        bool arm_STMIB(common::cc_flags cond, bool W, reg_index n, reg_list list);

        bool arm_LDR_lit(common::cc_flags cond, bool U, reg_index t, std::uint16_t imm12);
        bool arm_LDR_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index d, std::uint16_t imm12);
        bool arm_LDR_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index d, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m);
        bool arm_LDRB_lit(common::cc_flags cond, bool U, reg_index t, std::uint16_t imm12);
        bool arm_LDRB_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint16_t imm12);
        bool arm_LDRB_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m);
        bool arm_LDRH_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm8a, std::uint8_t imm8b);
        bool arm_LDRH_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index d, reg_index m);
        bool arm_LDRSB_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm8a, std::uint8_t imm8b);
        bool arm_LDRSH_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm8a, std::uint8_t imm8b);
        bool arm_STR_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint16_t imm12);
        bool arm_STR_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m);
        bool arm_STRB_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint16_t imm12);
        bool arm_STRB_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m);
        bool arm_STRH_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm8a, std::uint8_t imm8b);
        bool arm_STRH_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, reg_index m);

        // Multiply
        bool arm_MUL(common::cc_flags cond, bool S, reg_index d, reg_index m, reg_index n);
        bool arm_MLA(common::cc_flags cond, bool S, reg_index d, reg_index a, reg_index m, reg_index n);
        bool arm_UMLAL(common::cc_flags cond, bool S, reg_index d_hi, reg_index d_lo, reg_index m, reg_index n);
        bool arm_UMULL(common::cc_flags cond, bool S, reg_index d_hi, reg_index d_lo, reg_index m, reg_index n);
        bool arm_SMULL(common::cc_flags cond, bool S, reg_index d_hi, reg_index d_lo, reg_index m, reg_index n);

        // Status register access
        bool arm_MRS(common::cc_flags cond, reg_index d);
        bool arm_MSR_imm(common::cc_flags cond, int mask, int rotate, std::uint8_t imm8);
        bool arm_MSR_reg(common::cc_flags cond, int mask, reg_index n);

        // Interrupts
        bool arm_SVC(common::cc_flags cond, const std::uint32_t n);
        bool arm_UDF();
    };
}