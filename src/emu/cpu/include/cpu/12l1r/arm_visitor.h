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

#include <common/armemitter.h>
#include <cpu/12l1r/common.h>
#include <cpu/12l1r/visit_session.h>

namespace eka2l1::arm::r12l1 {
    class arm_translate_visitor : public visit_session {
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
        bool arm_MVN_rsr(common::cc_flags cond, bool S, reg_index d, reg_index s, common::armgen::shift_type shift,
            reg_index m);
        bool arm_ADC_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_ADC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_ADC_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
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
        bool arm_SBC_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_SBC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_RSB_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_RSB_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
            common::armgen::shift_type shift, reg_index m);
        bool arm_RSB_rsr(common::cc_flags cond, bool S, reg_index n, reg_index d, reg_index s,
            common::armgen::shift_type shift, reg_index m);
        bool arm_RSC_imm(common::cc_flags cond, bool S, reg_index n, reg_index d, int rotate, std::uint8_t imm8);
        bool arm_RSC_reg(common::cc_flags cond, bool S, reg_index n, reg_index d, std::uint8_t imm5,
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
        bool arm_TST_rsr(common::cc_flags cond, reg_index n, reg_index s, common::armgen::shift_type shift, reg_index m);
        bool arm_TEQ_imm(common::cc_flags cond, reg_index n, int rotate, std::uint8_t imm8);
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
        bool arm_BLX_imm(bool H, std::uint32_t imm24);

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
        bool arm_LDRD_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm8a, std::uint8_t imm8b);
        bool arm_LDRD_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, reg_index m);
        bool arm_LDRB_lit(common::cc_flags cond, bool U, reg_index t, std::uint16_t imm12);
        bool arm_LDRB_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint16_t imm12);
        bool arm_LDRB_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m);
        bool arm_LDRH_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm8a, std::uint8_t imm8b);
        bool arm_LDRH_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index d, reg_index m);
        bool arm_LDRSB_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm8a, std::uint8_t imm8b);
        bool arm_LDRSB_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index d, reg_index m);
        bool arm_LDRSH_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm8a, std::uint8_t imm8b);
        bool arm_LDRSH_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index d, reg_index m);
        bool arm_STR_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint16_t imm12);
        bool arm_STR_reg(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm5, common::armgen::shift_type shift, reg_index m);
        bool arm_STRD_imm(common::cc_flags cond, bool P, bool U, bool W, reg_index n, reg_index t, std::uint8_t imm8a, std::uint8_t imm8b);
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
        bool arm_SMLAL(common::cc_flags cond, bool S, reg_index d_hi, reg_index d_lo, reg_index m, reg_index n);
        bool arm_SMULxy(common::cc_flags cond, reg_index d, reg_index m, bool M, bool N, reg_index n);
        bool arm_SMLAxy(common::cc_flags cond, reg_index d, reg_index a, reg_index m, bool M, bool N, reg_index n);
        bool arm_SMULWy(common::cc_flags cond, reg_index d, reg_index m, bool top, reg_index n);
        bool arm_SMLAWy(common::cc_flags cond, reg_index d, reg_index a, reg_index m, bool top, reg_index n);

        // Status register access
        bool arm_MRS(common::cc_flags cond, reg_index d);
        bool arm_MSR_imm(common::cc_flags cond, int mask, int rotate, std::uint8_t imm8);
        bool arm_MSR_reg(common::cc_flags cond, int mask, reg_index n);

        // Coprocessors
        bool arm_MCR(common::cc_flags cond, std::size_t opc1, int cr_n, reg_index t, std::size_t coproc_no,
                     std::size_t opc2, int cr_m);
        bool arm_MRC(common::cc_flags cond, std::size_t opc1, int cr_n, reg_index t, std::size_t coproc_no,
                     size_t opc2, int cr_m);

        // Synchronization
        bool arm_STREX(common::cc_flags cond, reg_index n, reg_index d, reg_index t);
        bool arm_LDREX(common::cc_flags cond, reg_index n, reg_index t);
        bool arm_SWP(common::cc_flags cond, reg_index n, reg_index t, reg_index t2);

        // Miscs instructions
        bool arm_CLZ(common::cc_flags cond, reg_index d, reg_index m);
        bool arm_SXTH(common::cc_flags cond, reg_index d, std::uint8_t rotate_base_8, reg_index m);
        bool arm_UXTH(common::cc_flags cond, reg_index d, std::uint8_t rotate_base_8, reg_index m);
        bool arm_SXTB(common::cc_flags cond, reg_index d, std::uint8_t rotate_base_8, reg_index m);
        bool arm_UXTB(common::cc_flags cond, reg_index d, std::uint8_t rotate_base_8, reg_index m);
        bool arm_SEL(common::cc_flags cond, reg_index n, reg_index d, reg_index m);
        bool arm_PKHBT(common::cc_flags cond, reg_index n, reg_index d, std::uint8_t imm5, reg_index m);
        bool arm_PKHTB(common::cc_flags cond, reg_index n, reg_index d, std::uint8_t imm5, reg_index m);

        // Hint
        bool arm_PLD_imm(bool add, bool R, reg_index n, std::uint16_t imm12);
        bool arm_PLD_reg(bool add, bool R, reg_index n, std::uint8_t imm5, common::armgen::shift_type shift,
            reg_index m);

        // Interrupts
        bool arm_SVC(common::cc_flags cond, const std::uint32_t n);
        bool arm_BKPT();
        bool arm_UDF();

        // Saturated Add/Subtract instructions
        bool arm_QADD(common::cc_flags cond, reg_index n, reg_index d, reg_index m);
        bool arm_QSUB(common::cc_flags cond, reg_index n, reg_index d, reg_index m);
        bool arm_QDADD(common::cc_flags cond, reg_index n, reg_index d, reg_index m);
        bool arm_QDSUB(common::cc_flags cond, reg_index n, reg_index d, reg_index m);

        // VFP
        bool vfp_VMOV_reg(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool M, std::size_t Vm);
        bool vfp_VMOV_u32_f32(common::cc_flags cond, const std::size_t Vn, reg_index t, bool N);
        bool vfp_VMOV_f32_u32(common::cc_flags cond, const std::size_t Vn, reg_index t, bool N);
        bool vfp_VCMP(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool E, bool M, std::size_t Vm);
        bool vfp_VCVT_to_u32(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool round_towards_zero, bool M, std::size_t Vm);
        bool vfp_VCVT_to_s32(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool round_towards_zero, bool M, std::size_t Vm);
        bool vfp_VCVT_from_int(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool is_signed, bool M, std::size_t Vm);
        bool vfp_VCVT_f_to_f(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool M, std::size_t Vm);
        bool vfp_VMOV_f64_2u32(common::cc_flags cond, reg_index t2, reg_index t, bool M, std::size_t Vm);
        bool vfp_VMOV_2u32_f64(common::cc_flags cond, reg_index t2, reg_index t, bool M, std::size_t Vm);
        bool vfp_VADD(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm);
        bool vfp_VSUB(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm);
        bool vfp_VMUL(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm);
        bool vfp_VMLA(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm);
        bool vfp_VMLS(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm);
        bool vfp_VNMUL(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm);
        bool vfp_VNMLA(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm);
        bool vfp_VNMLS(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm);
        bool vfp_VDIV(common::cc_flags cond, bool D, std::size_t Vn, std::size_t Vd, bool sz, bool N, bool M, std::size_t Vm);
        bool vfp_VNEG(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool M, std::size_t Vm);
        bool vfp_VABS(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool M, std::size_t Vm);
        bool vfp_VSQRT(common::cc_flags cond, bool D, std::size_t Vd, bool sz, bool M, std::size_t Vm);
        bool vfp_VLDR(common::cc_flags cond, bool U, bool D, reg_index n, std::size_t Vd, bool sz, std::uint8_t imm8);
        bool vfp_VSTR(common::cc_flags cond, bool U, bool D, reg_index n, std::size_t Vd, bool sz, std::uint8_t imm8);
        bool vfp_VPUSH(common::cc_flags cond, bool D, std::size_t Vd, bool sz, std::uint8_t imm8);
        bool vfp_VPOP(common::cc_flags cond, bool D, std::size_t Vd, bool sz, std::uint8_t imm8);
        bool vfp_VSTM_a1(common::cc_flags cond, bool p, bool u, bool D, bool w, reg_index n, std::size_t Vd, std::uint8_t imm8);
        bool vfp_VSTM_a2(common::cc_flags cond, bool p, bool u, bool D, bool w, reg_index n, std::size_t Vd, std::uint8_t imm8);
        bool vfp_VLDM_a1(common::cc_flags cond, bool p, bool u, bool D, bool w, reg_index n, std::size_t Vd, std::uint8_t imm8);
        bool vfp_VLDM_a2(common::cc_flags cond, bool p, bool u, bool D, bool w, reg_index n, std::size_t Vd, std::uint8_t imm8);
        bool vfp_VMRS(common::cc_flags cond, reg_index t);
        bool vfp_VMSR(common::cc_flags cond, reg_index t);
    };
}