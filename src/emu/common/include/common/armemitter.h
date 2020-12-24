// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#pragma once

#include <cstdint>
#include <vector>

#include <common/armcommon.h>
#include <common/codeblock.h>
#include <common/cpudetect.h>
#include <common/log.h>

// VCVT flags
#define TO_FLOAT 0
#define TO_INT 1 << 0
#define IS_SIGNED 1 << 1
#define ROUND_TO_ZERO 1 << 2

namespace eka2l1::common::armgen {
    enum arm_reg {
        // GPRs
        R0 = 0,
        R1,
        R2,
        R3,
        R4,
        R5,
        R6,
        R7,
        R8,
        R9,
        R10,
        R11,

        // SPRs
        // R13 - R15 are SP, LR, and PC.
        // Almost always referred to by name instead of register number
        R12 = 12,
        R13 = 13,
        R14 = 14,
        R15 = 15,
        R_IP = 12,
        R_SP = 13,
        R_LR = 14,
        R_PC = 15,

        // VFP single precision registers
        S0,
        S1,
        S2,
        S3,
        S4,
        S5,
        S6,
        S7,
        S8,
        S9,
        S10,
        S11,
        S12,
        S13,
        S14,
        S15,
        S16,
        S17,
        S18,
        S19,
        S20,
        S21,
        S22,
        S23,
        S24,
        S25,
        S26,
        S27,
        S28,
        S29,
        S30,
        S31,

        // VFP Double Precision registers
        D0,
        D1,
        D2,
        D3,
        D4,
        D5,
        D6,
        D7,
        D8,
        D9,
        D10,
        D11,
        D12,
        D13,
        D14,
        D15,
        D16,
        D17,
        D18,
        D19,
        D20,
        D21,
        D22,
        D23,
        D24,
        D25,
        D26,
        D27,
        D28,
        D29,
        D30,
        D31,

        // ASIMD Quad-Word registers
        Q0,
        Q1,
        Q2,
        Q3,
        Q4,
        Q5,
        Q6,
        Q7,
        Q8,
        Q9,
        Q10,
        Q11,
        Q12,
        Q13,
        Q14,
        Q15,

        // for NEON VLD/VST instructions
        REG_UPDATE = R13,
        INVALID_REG = 0xFFFFFFFF
    };

    enum shift_type {
        ST_LSL = 0,
        ST_ASL = 0,
        ST_LSR = 1,
        ST_ASR = 2,
        ST_ROR = 3,
        ST_RRX = 4
    };
    enum integer_size {
        I_I8 = 0,
        I_I16,
        I_I32,
        I_I64
    };

    enum {
        NUMGPRs = 13,
    };

    class armx_emitter;

    enum op_type {
        TYPE_IMM = 0,
        TYPE_REG,
        TYPE_IMMSREG,
        TYPE_RSR,
        TYPE_MEM
    };

    // This is no longer a proper operand2 class. Need to split up.
    class operand2 {
        friend class armx_emitter;

    protected:
        std::uint32_t value;

    private:
        op_type type;

        // IMM types
        std::uint8_t rotation; // Only for std::uint8_t values

        // Register types
        std::uint8_t index_or_shift;
        shift_type shift;

    public:
        op_type get_type() const {
            return type;
        }

        shift_type get_shift_type() const {
            return shift;
        }

        std::uint8_t get_shift_value() const {
            return index_or_shift;
        }

        operand2() {}
        operand2(std::uint32_t imm, op_type otype = TYPE_IMM) {
            type = otype;
            value = imm;
            rotation = 0;
        }

        operand2(arm_reg Reg) {
            type = TYPE_REG;
            value = Reg;
            rotation = 0;
        }

        operand2(std::uint8_t imm, std::uint8_t the_rotation) {
            type = TYPE_IMM;
            value = imm;
            rotation = the_rotation;
        }

        operand2(arm_reg base, shift_type stype, arm_reg shift_reg) // RSR
        {
            type = TYPE_RSR;
            LOG_ERROR_IF(COMMON, !(type != ST_RRX), "Invalid operand2: RRX does not take a register shift amount");
            index_or_shift = shift_reg;
            shift = stype;
            value = base;
        }

        operand2(arm_reg base, shift_type stype, std::uint8_t shift) // For IMM shifted register
        {
            if (shift == 32)
                shift = 0;
            switch (stype) {
            case ST_LSL:
                LOG_ERROR_IF(COMMON, shift >= 32, "Invalid operand2: LSL %u", shift);
                break;
            case ST_LSR:
                LOG_ERROR_IF(COMMON, shift > 32, "Invalid operand2: LSR %u", shift);
                if (!shift)
                    stype = ST_LSL;
                if (shift == 32)
                    shift = 0;
                break;
            case ST_ASR:
                LOG_ERROR_IF(COMMON, shift >= 32, "Invalid operand2: ASR %u", shift);
                if (!shift)
                    stype = ST_LSL;
                if (shift == 32)
                    shift = 0;
                break;
            case ST_ROR:
                LOG_ERROR_IF(COMMON, shift >= 32, "Invalid operand2: ROR %u", shift);
                if (!shift)
                    stype = ST_LSL;
                break;
            case ST_RRX:
                LOG_ERROR_IF(COMMON, !(shift == 0), "Invalid operand2: RRX does not take an immediate shift amount");
                stype = ST_ROR;
                break;
            }
            index_or_shift = shift;
            shift = stype;
            value = base;
            type = TYPE_IMMSREG;
        }

        std::uint32_t get_data() {
            switch (type) {
            case TYPE_IMM:
                return Imm12Mod(); // This'll need to be changed later
            case TYPE_REG:
                return Rm();
            case TYPE_IMMSREG:
                return IMMSR();
            case TYPE_RSR:
                return RSR();
            default:
                LOG_ERROR_IF(COMMON, !(false), "GetData with Invalid Type");
                return 0;
            }
        }

        std::uint32_t get_raw_value() const {
            return value;
        }

        std::uint8_t get_rotation() const {
            return rotation;
        }

        std::uint32_t IMMSR() // IMM shifted register
        {
            LOG_ERROR_IF(COMMON, !(type == TYPE_IMMSREG), "IMMSR must be imm shifted register");
            return ((index_or_shift & 0x1f) << 7 | (shift << 5) | value);
        }

        std::uint32_t RSR() // Register shifted register
        {
            LOG_ERROR_IF(COMMON, !(type == TYPE_RSR), "RSR must be RSR Of Course");
            return (index_or_shift << 8) | (shift << 5) | 0x10 | value;
        }

        std::uint32_t Rm() const {
            LOG_ERROR_IF(COMMON, !(type == TYPE_REG), "Rm must be with Reg");
            return value;
        }

        std::uint32_t Imm5() const {
            LOG_ERROR_IF(COMMON, !((type == TYPE_IMM)), "Imm5 not IMM value");
            return ((value & 0x0000001F) << 7);
        }

        std::uint32_t Imm8() const {
            LOG_ERROR_IF(COMMON, !((type == TYPE_IMM)), "Imm8Rot not IMM value");
            return value & 0xFF;
        }

        std::uint32_t Imm8Rot() const // IMM8 with Rotation
        {
            LOG_ERROR_IF(COMMON, !((type == TYPE_IMM)), "Imm8Rot not IMM value");
            LOG_ERROR_IF(COMMON, (rotation & 0xE1) == 0, "Invalid operand2: immediate rotation %u", rotation);
            return (1 << 25) | (rotation << 7) | (value & 0x000000FF);
        }

        std::uint32_t Imm12() const {
            LOG_ERROR_IF(COMMON, !((type == TYPE_IMM)), "Imm12 not IMM");
            return (value & 0x00000FFF);
        }

        std::uint32_t Imm12Mod() const {
            // This is an IMM12 with the top four bits being rotation and the
            // bottom eight being an IMM. This is for instructions that need to
            // expand a 8bit IMM to a 32bit value and gives you some rotation as
            // well.
            // Each rotation rotates to the right by 2 bits
            LOG_ERROR_IF(COMMON, !((type == TYPE_IMM)), "Imm12Mod not IMM");
            return ((rotation & 0xF) << 8) | (value & 0xFF);
        }

        std::uint32_t Imm16() const {
            LOG_ERROR_IF(COMMON, !((type == TYPE_IMM)), "Imm16 not IMM");
            return ((value & 0xF000) << 4) | (value & 0x0FFF);
        }

        std::uint32_t Imm16Low() const {
            return Imm16();
        }

        std::uint32_t Imm16High() const // Returns high 16bits
        {
            LOG_ERROR_IF(COMMON, !((type == TYPE_IMM)), "Imm16 not IMM");
            return (((value >> 16) & 0xF000) << 4) | ((value >> 16) & 0x0FFF);
        }

        std::uint32_t Imm24() const {
            LOG_ERROR_IF(COMMON, !((type == TYPE_IMM)), "Imm16 not IMM");
            return (value & 0x0FFFFFFF);
        }

        // NEON and ASIMD specific
        std::uint32_t Imm8ASIMD() const {
            LOG_ERROR_IF(COMMON, !((type == TYPE_IMM)), "Imm8ASIMD not IMM");
            return ((value & 0x80) << 17) | ((value & 0x70) << 12) | (value & 0xF);
        }

        std::uint32_t Imm8VFP() const {
            LOG_ERROR_IF(COMMON, !((type == TYPE_IMM)), "Imm8VFP not IMM");
            return ((value & 0xF0) << 12) | (value & 0xF);
        }
    };

    // Use these when you don't know if an imm can be represented as an operand2.
    // This lets you generate both an optimal and a fallback solution by checking
    // the return value, which will be false if these fail to find a operand2 that
    // represents your 32-bit imm value.
    bool try_make_operand2(std::uint32_t imm, operand2 &op2);
    bool try_make_operand2_allow_inverse(std::uint32_t imm, operand2 &op2, bool *inverse);
    bool try_make_operand2_allow_negation(std::int32_t imm, operand2 &op2, bool *negated);

    // Use this only when you know imm can be made into an operand2.
    operand2 assume_make_operand2(std::uint32_t imm);

    inline operand2 R(arm_reg Reg) { return operand2(Reg, TYPE_REG); }
    inline operand2 IMM(std::uint32_t Imm) { return operand2(Imm, TYPE_IMM); }
    inline operand2 Mem(void *ptr) { return operand2((std::uint32_t)(uintptr_t)ptr, TYPE_IMM); }
//usage: struct {int e;} s; STRUCT_OFFSET(s,e)
#define STRUCT_OFF(str, elem) ((std::uint32_t)((std::uint32_t) & (str).elem - (std::uint32_t) & (str)))

    struct fixup_branch {
        std::uint8_t *ptr;
        std::uint32_t condition; // Remembers our codition at the time
        int type; //0 = B 1 = BL
    };

    struct literal_pool {
        intptr_t loc;
        std::uint8_t *ldr_address;
        std::uint32_t val;
    };

    typedef const std::uint8_t *jump_target;

    // XXX: Stop polluting the global namespace
    const std::uint32_t I_8 = (1 << 0);
    const std::uint32_t I_16 = (1 << 1);
    const std::uint32_t I_32 = (1 << 2);
    const std::uint32_t I_64 = (1 << 3);
    const std::uint32_t I_SIGNED = (1 << 4);
    const std::uint32_t I_UNSIGNED = (1 << 5);
    const std::uint32_t F_32 = (1 << 6);
    const std::uint32_t I_POLYNOMIAL = (1 << 7); // Only used in VMUL/VMULL

    enum VIMMMode {
        VIMM___x___x = 0x0, // 0000 VMOV
        VIMM__x___x_ = 0x2, // 0010
        VIMM_x___x__ = 0x4, // 0100
        VIMMx___x___ = 0x6, // 0110
        VIMM_x_x_x_x = 0x8, // 1000
        VIMMx_x_x_x_ = 0xA, // 1010
        VIMM__x1__x1 = 0xC, // 1100
        VIMM_x11_x11 = 0xD, // 1101
        VIMMxxxxxxxx = 0xE, // 1110  // op == 0
        VIMMf000f000 = 0xF, // 1111  // op == 0     ( really   aBbbbbbc defgh 00000000 00000000 ) where B = NOT b
        VIMMbits2bytes = 0x1E, // Bit replication into bytes! Easily created 111111111 00000000 masks!
    };

    std::uint32_t encode_vd(arm_reg Vd);
    std::uint32_t encode_vn(arm_reg Vn);
    std::uint32_t encode_vm(arm_reg Vm);

    std::uint32_t encoded_size(std::uint32_t value);

    // Subtracts the base from the register to give us the real one
    arm_reg subbase(arm_reg Reg);

    inline bool is_q(arm_reg r) {
        return r >= Q0 && r <= Q15;
    }

    inline bool is_d(arm_reg r) {
        return r >= D0 && r <= D31;
    }

    // See A.7.1 in the ARMv7-A
    // VMUL F32 scalars can only be up to D15[0], D15[1] - higher scalars cannot be individually addressed
    arm_reg DScalar(arm_reg dreg, int subScalar);
    arm_reg QScalar(arm_reg qreg, int subScalar);

    inline arm_reg XScalar(arm_reg reg, int subScalar) {
        if (is_q(reg))
            return QScalar(reg, subScalar);
        else
            return DScalar(reg, subScalar);
    }

    const char *arm_reg_as_string(arm_reg reg);

    // Get the two halves of a Q register.
    inline arm_reg D_0(arm_reg q) {
        if (q >= Q0 && q <= Q15) {
            return arm_reg(D0 + (q - Q0) * 2);
        } else if (q >= D0 && q <= D31) {
            return q;
        } else {
            return INVALID_REG;
        }
    }

    inline arm_reg D_1(arm_reg q) {
        return arm_reg(D0 + (q - Q0) * 2 + 1);
    }

    enum NEONAlignment {
        ALIGN_NONE = 0,
        ALIGN_64 = 1,
        ALIGN_128 = 2,
        ALIGN_256 = 3
    };

    class NEONXEmitter;

    class armx_emitter {
        friend struct OpArg; // for Write8 etc
        friend class NEONXEmitter;

    protected:
        cpu_info context_info;

    private:
        std::uint8_t *code, *startcode;
        std::uint8_t *last_cache_flush_end;
        std::uint32_t condition;
        std::vector<literal_pool> current_lit_pool;

        void write_store_op(std::uint32_t Op, arm_reg Rt, arm_reg Rn, operand2 op2, bool RegAdd);
        void write_reg_store_op(std::uint32_t op, arm_reg dest, bool WriteBack, std::uint16_t RegList);
        void write_vreg_store_op(std::uint32_t op, arm_reg dest, bool Double, bool WriteBack, arm_reg firstreg, std::uint8_t numregs);
        void write_shifted_data_op(std::uint32_t op, bool SetFlags, arm_reg dest, arm_reg src, arm_reg op2);
        void write_shifted_data_op(std::uint32_t op, bool SetFlags, arm_reg dest, arm_reg src, operand2 op2);
        void write_signed_multiply(std::uint32_t Op, std::uint32_t Op2, std::uint32_t Op3, arm_reg dest, arm_reg r1, arm_reg r2);

        void write_vfp_data_op(std::uint32_t Op, arm_reg Vd, arm_reg Vn, arm_reg Vm);

        void write_4op_multiply(std::uint32_t op, arm_reg destLo, arm_reg destHi, arm_reg rn, arm_reg rm);

        // New Ops
        void write_instruction(std::uint32_t op, arm_reg Rd, arm_reg Rn, operand2 Rm, bool SetFlags = false);

        void write_vldst1(bool load, std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align, arm_reg Rm);
        void write_vldst1_lane(bool load, std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, bool aligned, arm_reg Rm);

        void write_vimm(arm_reg Vd, int cmode, std::uint8_t imm, int op);

        void encode_shift_by_imm(std::uint32_t Size, arm_reg Vd, arm_reg Vm, int shiftAmount, std::uint8_t opcode, bool quad, bool inverse, bool halve);

    protected:
        inline void write32(std::uint32_t value) {
            *(std::uint32_t *)code = value;
            code += 4;
        }

    public:
        explicit armx_emitter()
            : code(0)
            , startcode(0)
            , last_cache_flush_end(0) {
            condition = CC_AL << 28;
        }

        explicit armx_emitter(std::uint8_t *code_ptr, cpu_info context_info)
            : context_info(context_info) {
            code = code_ptr;
            last_cache_flush_end = code_ptr;
            startcode = code_ptr;
            condition = CC_AL << 28;
        }

        virtual ~armx_emitter() {}

        void set_code_pointer(std::uint8_t *ptr);
        const std::uint8_t *get_code_pointer() const;

        void reserve_codespace(std::uint32_t bytes);
        const std::uint8_t *align_code16();
        const std::uint8_t *align_code_page();
        void flush_icache();
        void flush_icache_section(std::uint8_t *start, std::uint8_t *end);
        std::uint8_t *get_writeable_code_ptr();

        void flush_lit_pool();
        void add_new_lit(std::uint32_t val);
        bool try_set_value_two_op(arm_reg reg, std::uint32_t val);

        cc_flags get_cc() { return cc_flags(condition >> 28); }
        void set_cc(cc_flags cond = CC_AL);

        // Special purpose instructions

        // Dynamic Endian Switching
        void SETEND(bool BE);
        // Debug Breakpoint
        void BKPT(std::uint16_t arg);

        // Hint instruction
        void YIELD();

        // Do nothing
        void NOP(int count = 1); //nop padding - TODO: fast nop slides, for amd and intel (check their manuals)

#ifdef CALL
#undef CALL
#endif

        // Branching
        fixup_branch B();
        fixup_branch B_CC(cc_flags Cond);
        void B_CC(cc_flags Cond, const void *fnptr);
        fixup_branch BL();
        fixup_branch BL_CC(cc_flags Cond);
        void set_jump_target(fixup_branch const &branch);

        void B(const void *fnptr);
        void B(arm_reg src);
        void BL(const void *fnptr);
        void BL(arm_reg src);
        bool BLInRange(const void *fnptr) const;

        void PUSH(const int num, ...);
        void POP(const int num, ...);

        // New Data Ops
        void AND(arm_reg Rd, arm_reg Rn, operand2 Rm);
        void ANDS(arm_reg Rd, arm_reg Rn, operand2 Rm);
        void EOR(arm_reg dest, arm_reg src, operand2 op2);
        void EORS(arm_reg dest, arm_reg src, operand2 op2);
        void SUB(arm_reg dest, arm_reg src, operand2 op2);
        void SUBS(arm_reg dest, arm_reg src, operand2 op2);
        void RSB(arm_reg dest, arm_reg src, operand2 op2);
        void RSBS(arm_reg dest, arm_reg src, operand2 op2);
        void ADD(arm_reg dest, arm_reg src, operand2 op2);
        void ADDS(arm_reg dest, arm_reg src, operand2 op2);
        void ADC(arm_reg dest, arm_reg src, operand2 op2);
        void ADCS(arm_reg dest, arm_reg src, operand2 op2);
        void LSL(arm_reg dest, arm_reg src, operand2 op2);
        void LSL(arm_reg dest, arm_reg src, arm_reg op2);
        void LSLS(arm_reg dest, arm_reg src, operand2 op2);
        void LSLS(arm_reg dest, arm_reg src, arm_reg op2);
        void LSR(arm_reg dest, arm_reg src, operand2 op2);
        void LSRS(arm_reg dest, arm_reg src, operand2 op2);
        void LSR(arm_reg dest, arm_reg src, arm_reg op2);
        void LSRS(arm_reg dest, arm_reg src, arm_reg op2);
        void ASR(arm_reg dest, arm_reg src, operand2 op2);
        void ASRS(arm_reg dest, arm_reg src, operand2 op2);
        void ASR(arm_reg dest, arm_reg src, arm_reg op2);
        void ASRS(arm_reg dest, arm_reg src, arm_reg op2);

        void SBC(arm_reg dest, arm_reg src, operand2 op2);
        void SBCS(arm_reg dest, arm_reg src, operand2 op2);
        void RBIT(arm_reg dest, arm_reg src);
        void REV(arm_reg dest, arm_reg src);
        void REV16(arm_reg dest, arm_reg src);
        void RSC(arm_reg dest, arm_reg src, operand2 op2);
        void RSCS(arm_reg dest, arm_reg src, operand2 op2);
        void TST(arm_reg src, operand2 op2);
        void TEQ(arm_reg src, operand2 op2);
        void CMP(arm_reg src, operand2 op2);
        void CMN(arm_reg src, operand2 op2);
        void ORR(arm_reg dest, arm_reg src, operand2 op2);
        void ORRS(arm_reg dest, arm_reg src, operand2 op2);
        void MOV(arm_reg dest, operand2 op2);
        void MOVS(arm_reg dest, operand2 op2);
        void BIC(arm_reg dest, arm_reg src, operand2 op2); // BIC = ANDN
        void BICS(arm_reg dest, arm_reg src, operand2 op2);
        void MVN(arm_reg dest, operand2 op2);
        void MVNS(arm_reg dest, operand2 op2);
        void MOVW(arm_reg dest, operand2 op2);
        void MOVT(arm_reg dest, operand2 op2, bool TopBits = false);

        // UDIV and SDIV are only available on CPUs that have
        // the idiva hardare capacity
        void UDIV(arm_reg dest, arm_reg dividend, arm_reg divisor);
        void SDIV(arm_reg dest, arm_reg dividend, arm_reg divisor);

        void MUL(arm_reg dest, arm_reg src, arm_reg op2);
        void MULS(arm_reg dest, arm_reg src, arm_reg op2);

        void UMULL(arm_reg destLo, arm_reg destHi, arm_reg rn, arm_reg rm);
        void SMULL(arm_reg destLo, arm_reg destHi, arm_reg rn, arm_reg rm);

        void UMLAL(arm_reg destLo, arm_reg destHi, arm_reg rn, arm_reg rm);
        void SMLAL(arm_reg destLo, arm_reg destHi, arm_reg rn, arm_reg rm);

        void SXTB(arm_reg dest, arm_reg op2);
        void SXTH(arm_reg dest, arm_reg op2, std::uint8_t rotation = 0);
        void SXTAH(arm_reg dest, arm_reg src, arm_reg op2, std::uint8_t rotation = 0);
        void BFI(arm_reg rd, arm_reg rn, std::uint8_t lsb, std::uint8_t width);
        void BFC(arm_reg rd, std::uint8_t lsb, std::uint8_t width);
        void UBFX(arm_reg dest, arm_reg op2, std::uint8_t lsb, std::uint8_t width);
        void SBFX(arm_reg dest, arm_reg op2, std::uint8_t lsb, std::uint8_t width);
        void CLZ(arm_reg rd, arm_reg rm);
        void PLD(arm_reg rd, int offset, bool forWrite = false);

        // Using just MSR here messes with our defines on the PPC side of stuff (when this code was in dolphin...)
        // Just need to put an underscore here, bit annoying.
        void _MSR(bool nzcvq, bool g, operand2 op2);
        void _MSR(bool nzcvq, bool g, arm_reg src);
        void MRS(arm_reg dest);

        // Memory load/store operations
        void LDR(arm_reg dest, arm_reg base, operand2 op2 = 0, bool RegAdd = true);
        void LDRB(arm_reg dest, arm_reg base, operand2 op2 = 0, bool RegAdd = true);
        void LDRH(arm_reg dest, arm_reg base, operand2 op2 = 0, bool RegAdd = true);
        void LDRSB(arm_reg dest, arm_reg base, operand2 op2 = 0, bool RegAdd = true);
        void LDRSH(arm_reg dest, arm_reg base, operand2 op2 = 0, bool RegAdd = true);
        void LDRLIT(arm_reg dest, std::uint32_t offset, bool Add = true);
        void STR(arm_reg result, arm_reg base, operand2 op2 = 0, bool RegAdd = true);
        void STRB(arm_reg result, arm_reg base, operand2 op2 = 0, bool RegAdd = true);
        void STRH(arm_reg result, arm_reg base, operand2 op2 = 0, bool RegAdd = true);

        void STMFD(arm_reg dest, bool WriteBack, const int Regnum, ...);
        void LDMFD(arm_reg dest, bool WriteBack, const int Regnum, ...);
        void STMIA(arm_reg dest, bool WriteBack, const int Regnum, ...);
        void LDMIA(arm_reg dest, bool WriteBack, const int Regnum, ...);
        void STM(arm_reg dest, bool Add, bool Before, bool WriteBack, const int Regnum, ...);
        void LDM(arm_reg dest, bool Add, bool Before, bool WriteBack, const int Regnum, ...);
        void STMBitmask(arm_reg dest, bool Add, bool Before, bool WriteBack, const std::uint16_t RegList);
        void LDMBitmask(arm_reg dest, bool Add, bool Before, bool WriteBack, const std::uint16_t RegList);

        // Exclusive Access operations
        void LDREX(arm_reg dest, arm_reg base);
        // result contains the result if the instruction managed to store the value
        void STREX(arm_reg result, arm_reg base, arm_reg op);
        void DMB();
        void SVC(operand2 op);

        // NEON and ASIMD instructions
        // None of these will be created with conditional since ARM
        // is deprecating conditional execution of ASIMD instructions.
        // ASIMD instructions don't even have a conditional encoding.

        // NEON Only
        void VABD(integer_size size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VADD(integer_size size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VSUB(integer_size size, arm_reg Vd, arm_reg Vn, arm_reg Vm);

        // VFP Only
        void VLDMIA(arm_reg dest, bool WriteBack, arm_reg firstreg, int numregs);
        void VSTMIA(arm_reg dest, bool WriteBack, arm_reg firstreg, int numregs);
        void VLDMDB(arm_reg dest, bool WriteBack, arm_reg firstreg, int numregs);
        void VSTMDB(arm_reg dest, bool WriteBack, arm_reg firstreg, int numregs);
        void VPUSH(arm_reg firstvreg, int numvregs) {
            VSTMDB(R_SP, true, firstvreg, numvregs);
        }
        void VPOP(arm_reg firstvreg, int numvregs) {
            VLDMIA(R_SP, true, firstvreg, numvregs);
        }
        void VLDR(arm_reg Dest, arm_reg Base, std::int16_t offset);
        void VSTR(arm_reg Src, arm_reg Base, std::int16_t offset);
        void VCMP(arm_reg Vd, arm_reg Vm);
        void VCMPE(arm_reg Vd, arm_reg Vm);
        // Compares against zero
        void VCMP(arm_reg Vd);
        void VCMPE(arm_reg Vd);

        void VNMLA(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VNMLS(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VNMUL(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VDIV(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VSQRT(arm_reg Vd, arm_reg Vm);

        // NEON and VFP
        void VADD(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VSUB(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VABS(arm_reg Vd, arm_reg Vm);
        void VNEG(arm_reg Vd, arm_reg Vm);
        void VMUL(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMLA(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMLS(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMOV(arm_reg Dest, operand2 op2);
        void VMOV(arm_reg Dest, arm_reg Src, bool high);
        void VMOV(arm_reg Dest, arm_reg Src);
        // Either Vd, Rt, Rt2 or Rt, Rt2, Vd.
        void VMOV(arm_reg Dest, arm_reg Src1, arm_reg Src2);
        void VCVT(arm_reg Dest, arm_reg Src, int flags);

        // NEON, need to check for this (supported if VFP4 is supported)
        void VCVTF32F16(arm_reg Dest, arm_reg Src);
        void VCVTF16F32(arm_reg Dest, arm_reg Src);

        void VABA(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VABAL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VABD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VABDL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VABS(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VACGE(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VACGT(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VACLE(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VACLT(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VADD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VADDHN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VADDL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VADDW(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VBIF(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VBIT(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VBSL(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VCEQ(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VCEQ(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VCGE(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VCGE(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VCGT(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VCGT(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VCLE(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VCLE(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VCLS(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VCLT(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VCLT(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VCLZ(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VCNT(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VDUP(std::uint32_t Size, arm_reg Vd, arm_reg Vm, std::uint8_t index);
        void VDUP(std::uint32_t Size, arm_reg Vd, arm_reg Rt);
        void VEXT(arm_reg Vd, arm_reg Vn, arm_reg Vm, std::uint8_t index);
        void VFMA(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VFMS(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VHADD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VHSUB(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMAX(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMIN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);

        // Three registers
        void VMLA(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMLS(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMLAL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMLSL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMUL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMULL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQDMLAL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQDMLSL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQDMULH(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQDMULL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQRDMULH(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);

        // Two registers and a scalar
        // These two are super useful for matrix multiplication
        void VMUL_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMLA_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);

        // TODO:
        /*
        void VMLS_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMLAL_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMLSL_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VMULL_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQDMLAL_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQDMLSL_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQDMULH_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQDMULL_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQRDMULH_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        */

        // Vector bitwise. These don't have an element size for obvious reasons.
        void VAND(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VBIC(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VEOR(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VORN(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VORR(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        inline void VMOV_neon(arm_reg Dest, arm_reg Src) {
            VORR(Dest, Src, Src);
        }
        void VMOV_neon(std::uint32_t Size, arm_reg Vd, std::uint32_t imm);
        void VMOV_neon(std::uint32_t Size, arm_reg Vd, float imm) {
            LOG_ERROR_IF(COMMON, !(Size == F_32), "Expecting F_32 immediate for VMOV_neon float arg.");
            union {
                float f;
                std::uint32_t u;
            } val;
            val.f = imm;
            VMOV_neon(I_32, Vd, val.u);
        }
        void VMOV_neon(std::uint32_t Size, arm_reg Vd, arm_reg Rt, int lane);

        void VNEG(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VMVN(arm_reg Vd, arm_reg Vm);
        void VPADAL(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VPADD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VPADDL(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VPMAX(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VPMIN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQABS(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VQADD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQNEG(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VQRSHL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQSHL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VQSUB(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VRADDHN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VRECPE(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VRECPS(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VRHADD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VRSHL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VRSQRTE(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VRSQRTS(arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VRSUBHN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VSHL(std::uint32_t Size, arm_reg Vd, arm_reg Vm, arm_reg Vn); // Register shift
        void VSUB(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VSUBHN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VSUBL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VSUBW(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VSWP(arm_reg Vd, arm_reg Vm);
        void VTRN(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VTST(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm);
        void VUZP(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VZIP(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VREVX(std::uint32_t size, std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VREV64(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VREV32(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VREV16(std::uint32_t Size, arm_reg Vd, arm_reg Vm);

        // NEON immediate instructions

        void VMOV_imm(std::uint32_t Size, arm_reg Vd, VIMMMode type, int imm);
        void VMOV_immf(arm_reg Vd, float value); // This only works with a select few values (1.0f and -1.0f).

        void VORR_imm(std::uint32_t Size, arm_reg Vd, VIMMMode type, int imm);
        void VMVN_imm(std::uint32_t Size, arm_reg Vd, VIMMMode type, int imm);
        void VBIC_imm(std::uint32_t Size, arm_reg Vd, VIMMMode type, int imm);

        // Widening and narrowing moves
        void VMOVL(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VMOVN(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VQMOVN(std::uint32_t Size, arm_reg Vd, arm_reg Vm);
        void VQMOVUN(std::uint32_t Size, arm_reg Vd, arm_reg Vm);

        // Shifts by immediate
        void VSHL(std::uint32_t Size, arm_reg Vd, arm_reg Vm, int shiftAmount);
        void VSHLL(std::uint32_t Size, arm_reg Vd, arm_reg Vm, int shiftAmount); // widening
        void VSHR(std::uint32_t Size, arm_reg Vd, arm_reg Vm, int shiftAmount);
        void VSHRN(std::uint32_t Size, arm_reg Vd, arm_reg Vm, int shiftAmount); // narrowing

        // Vector VCVT
        void VCVT(std::uint32_t DestSize, arm_reg Dest, arm_reg Src);

        // Notes:
        // Rm == R_PC  is interpreted as no offset, otherwise, effective address is sum of Rn and Rm
        // Rm == R13  is interpreted as   VLD1,   ....  [Rn]!    Added a REG_UPDATE pseudo register.

        // Load/store multiple registers full of elements (a register is a D register)
        // Specifying alignment when it can be guaranteed is documented to improve load/store performance.
        // For example, when loading a set of four 64-bit registers that we know is 32-byte aligned, we should specify ALIGN_256.
        void VLD1(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align = ALIGN_NONE, arm_reg Rm = R_PC);
        void VST1(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align = ALIGN_NONE, arm_reg Rm = R_PC);

        // Load/store single lanes of D registers
        void VLD1_lane(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, bool aligned, arm_reg Rm = R_PC);
        void VST1_lane(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, bool aligned, arm_reg Rm = R_PC);

        // Load one value into all lanes of a D or a Q register (either supported, all formats should work).
        void VLD1_all_lanes(std::uint32_t Size, arm_reg Vd, arm_reg Rn, bool aligned, arm_reg Rm = R_PC);

        /*
        // Deinterleave two loads... or something. TODO
        void VLD2(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align = ALIGN_NONE, arm_reg Rm = R_PC);
        void VST2(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align = ALIGN_NONE, arm_reg Rm = R_PC);
        void VLD2_lane(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, arm_reg Rm = R_PC);
        void VST2_lane(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, arm_reg Rm = R_PC);
        void VLD3(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align = ALIGN_NONE, arm_reg Rm = R_PC);
        void VST3(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align = ALIGN_NONE, arm_reg Rm = R_PC);
        void VLD3_lane(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, arm_reg Rm = R_PC);
        void VST3_lane(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, arm_reg Rm = R_PC);
        void VLD4(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align = ALIGN_NONE, arm_reg Rm = R_PC);
        void VST4(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align = ALIGN_NONE, arm_reg Rm = R_PC);
        void VLD4_lane(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, arm_reg Rm = R_PC);
        void VST4_lane(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, arm_reg Rm = R_PC);
        */

        void VMRS_APSR();
        void VMRS(arm_reg Rt);
        void VMSR(arm_reg Rt);

        void quick_call_function(arm_reg scratchreg, const void *func);

        template <typename T>
        void quick_call_function(arm_reg scratchreg, T func) {
            quick_call_function(scratchreg, (const void *)func);
        }

        // Wrapper around MOVT/MOVW with fallbacks.
        void MOVI2R(arm_reg reg, std::uint32_t val, bool optimize = true);
        void MOVI2FR(arm_reg dest, float val, bool negate = false);
        void MOVI2F(arm_reg dest, float val, arm_reg tempReg, bool negate = false);
        void MOVI2F_neon(arm_reg dest, float val, arm_reg tempReg, bool negate = false);

        // Load pointers without casting
        template <class T>
        void MOVP2R(arm_reg reg, T *val) {
            MOVI2R(reg, (std::uint32_t)(uintptr_t)(void *)val);
        }

        void MOVIU2F(arm_reg dest, std::uint32_t val, arm_reg tempReg, bool negate = false) {
            union {
                std::uint32_t u;
                float f;
            } v = { val };
            MOVI2F(dest, v.f, tempReg, negate);
        }

        void ADDI2R(arm_reg rd, arm_reg rs, std::uint32_t val, arm_reg scratch);
        bool TryADDI2R(arm_reg rd, arm_reg rs, std::uint32_t val);
        void SUBI2R(arm_reg rd, arm_reg rs, std::uint32_t val, arm_reg scratch);
        bool TrySUBI2R(arm_reg rd, arm_reg rs, std::uint32_t val);
        void ANDI2R(arm_reg rd, arm_reg rs, std::uint32_t val, arm_reg scratch);
        bool TryANDI2R(arm_reg rd, arm_reg rs, std::uint32_t val);
        void CMPI2R(arm_reg rs, std::uint32_t val, arm_reg scratch);
        bool TryCMPI2R(arm_reg rs, std::uint32_t val);
        void TSTI2R(arm_reg rs, std::uint32_t val, arm_reg scratch);
        bool TryTSTI2R(arm_reg rs, std::uint32_t val);
        void ORI2R(arm_reg rd, arm_reg rs, std::uint32_t val, arm_reg scratch);
        bool TryORI2R(arm_reg rd, arm_reg rs, std::uint32_t val);
        void EORI2R(arm_reg rd, arm_reg rs, std::uint32_t val, arm_reg scratch);
        bool TryEORI2R(arm_reg rd, arm_reg rs, std::uint32_t val);
    }; // class armx_emitter

    // Everything that needs to generate machine code should inherit from this.
    // You get memory management for free, plus, you can use all the MOV etc functions without
    // having to prefix them with gen-> or something similar.

    class armx_codeblock : public common::codeblock<armx_emitter> {
    public:
        void poision_memory(int offset) override;
    };

    // VFP Specific
    struct VFPEnc {
        std::int16_t opc1;
        std::int16_t opc2;
    };
    extern const VFPEnc VFPOps[16][2];
    extern const char *VFPOpNames[16];

} // namespace