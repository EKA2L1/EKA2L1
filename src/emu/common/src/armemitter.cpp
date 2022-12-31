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

#include <common/log.h>
#include <common/platform.h>

#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if EKA2L1_PLATFORM(IOS)
#include <libkern/OSCacheControl.h>
#include <sys/mman.h>
#elif EKA2L1_PLATFORM(WIN32)
#include <Windows.h>
#endif

#include <common/armemitter.h>
#include <common/virtualmem.h>

namespace eka2l1::common::armgen {
    inline std::uint32_t rotr(std::uint32_t a, int amount) {
        if (!amount)
            return a;
        return (a >> amount) | (a << (32 - amount));
    }

    inline std::uint32_t rotl(std::uint32_t a, int amount) {
        if (!amount)
            return a;
        return (a << amount) | (a >> (32 - amount));
    }

    bool try_make_operand2(std::uint32_t imm, operand2 &op2) {
        // Just brute force it.
        for (int i = 0; i < 16; i++) {
            int mask = rotr(0xFF, i * 2);
            if ((imm & mask) == imm) {
                op2 = operand2((std::uint8_t)(rotl(imm, i * 2)), (std::uint8_t)i);
                return true;
            }
        }
        return false;
    }

    bool try_make_operand2_allow_inverse(std::uint32_t imm, operand2 &op2, bool *inverse) {
        if (!try_make_operand2(imm, op2)) {
            *inverse = true;
            return try_make_operand2(~imm, op2);
        } else {
            *inverse = false;
            return true;
        }
    }

    bool try_make_operand2_allow_negation(std::int32_t imm, operand2 &op2, bool *negated) {
        if (!try_make_operand2(imm, op2)) {
            *negated = true;
            return try_make_operand2(-imm, op2);
        } else {
            *negated = false;
            return true;
        }
    }

    operand2 assume_make_operand2(std::uint32_t imm) {
        operand2 op2;
        bool result = try_make_operand2(imm, op2);
        assert(result && "Could not make assumed operand2.");
        if (!result) {
            // Make double sure that we get it logged.
            LOG_ERROR(COMMON, "Could not make assumed operand2.");
        }
        return op2;
    }

    bool armx_emitter::try_set_value_two_op(arm_reg reg, std::uint32_t val) {
        int ops = 0;
        for (int i = 0; i < 16; i++) {
            if ((val >> (i * 2)) & 0x3) {
                ops++;
                i += 3;
            }
        }
        if (ops > 2)
            return false;

        bool first = true;
        for (int i = 0; i < 16; i++, val >>= 2) {
            if (val & 0x3) {
                first ? MOV(reg, operand2((std::uint8_t)val, (std::uint8_t)((16 - i) & 0xF)))
                      : ORR(reg, reg, operand2((std::uint8_t)val, (std::uint8_t)((16 - i) & 0xF)));
                first = false;
                i += 3;
                val >>= 6;
            }
        }
        return true;
    }

    bool TryMakeFloatIMM8(std::uint32_t val, operand2 &op2) {
        if ((val & 0x0007FFFF) == 0) {
            // VFP Encoding for Imms: <7> Not(<6>) Repeat(<6>,5) <5:0> Zeros(19)
            bool bit6 = (val & 0x40000000) == 0x40000000;
            bool canEncode = true;
            for (std::uint32_t mask = 0x20000000; mask >= 0x02000000; mask >>= 1) {
                if (((val & mask) == mask) == bit6)
                    canEncode = false;
            }
            if (canEncode) {
                std::uint32_t imm8 = (val & 0x80000000) >> 24; // sign bit
                imm8 |= (!bit6 << 6);
                imm8 |= (val & 0x01F80000) >> 19;
                op2 = IMM(imm8);
                return true;
            }
        }

        return false;
    }

    void armx_emitter::MOVI2FR(arm_reg dest, float val, bool negate) {
        union {
            float f;
            std::uint32_t u;
        } conv;
        conv.f = negate ? -val : val;
        MOVI2R(dest, conv.u);
    }

    void armx_emitter::MOVI2F(arm_reg dest, float val, arm_reg tempReg, bool negate) {
        union {
            float f;
            std::uint32_t u;
        } conv;
        conv.f = negate ? -val : val;
        // Try moving directly first if mantisse is empty
        operand2 op2;
        if (context_info.bVFPv3 && TryMakeFloatIMM8(conv.u, op2))
            VMOV(dest, op2);
        else {
            MOVI2R(tempReg, conv.u);
            VMOV(dest, tempReg);
        }
        // Otherwise, possible to use a literal pool and VLDR directly (+- 1020)
    }

    void armx_emitter::MOVI2F_neon(arm_reg dest, float val, arm_reg tempReg, bool negate) {
        union {
            float f;
            std::uint32_t u;
        } conv;
        conv.f = negate ? -val : val;
        // Try moving directly first if mantisse is empty
        operand2 op2;
        if (context_info.bVFPv3 && TryMakeFloatIMM8(conv.u, op2))
            VMOV_neon(F_32, dest, conv.u);
        else {
            MOVI2R(tempReg, conv.u);
            VDUP(F_32, dest, tempReg);
        }
        // Otherwise, possible to use a literal pool and VLD1 directly (+- 1020)
    }

    void armx_emitter::ADDI2R(arm_reg rd, arm_reg rs, std::uint32_t val, arm_reg scratch) {
        if (!TryADDI2R(rd, rs, val)) {
            MOVI2R(scratch, val);
            ADD(rd, rs, scratch);
        }
    }

    bool armx_emitter::TryADDI2R(arm_reg rd, arm_reg rs, std::uint32_t val) {
        if (val == 0) {
            if (rd != rs)
                MOV(rd, rs);
            return true;
        }
        operand2 op2;
        bool negated;
        if (try_make_operand2_allow_negation(val, op2, &negated)) {
            if (!negated)
                ADD(rd, rs, op2);
            else
                SUB(rd, rs, op2);
            return true;
        } else {
            // Try 16-bit additions and subtractions - easy to test for.
            // Should also try other rotations...
            if ((val & 0xFFFF0000) == 0) {
                // Decompose into two additions.
                ADD(rd, rs, operand2((std::uint8_t)(val >> 8), 12)); // rotation right by 12*2 == rotation left by 8
                ADD(rd, rd, operand2((std::uint8_t)(val), 0));
                return true;
            } else if ((((std::uint32_t) - (std::int32_t)val) & 0xFFFF0000) == 0) {
                val = (std::uint32_t) - (std::int32_t)val;
                SUB(rd, rs, operand2((std::uint8_t)(val >> 8), 12));
                SUB(rd, rd, operand2((std::uint8_t)(val), 0));
                return true;
            } else {
                return false;
            }
        }
    }

    void armx_emitter::SUBI2R(arm_reg rd, arm_reg rs, std::uint32_t val, arm_reg scratch) {
        if (!TrySUBI2R(rd, rs, val)) {
            MOVI2R(scratch, val);
            SUB(rd, rs, scratch);
        }
    }

    bool armx_emitter::TrySUBI2R(arm_reg rd, arm_reg rs, std::uint32_t val) {
        // Just add a negative.
        return TryADDI2R(rd, rs, (std::uint32_t) - (std::int32_t)val);
    }

    void armx_emitter::ANDI2R(arm_reg rd, arm_reg rs, std::uint32_t val, arm_reg scratch) {
        if (!TryANDI2R(rd, rs, val)) {
            MOVI2R(scratch, val);
            AND(rd, rs, scratch);
        }
    }

    bool armx_emitter::TryANDI2R(arm_reg rd, arm_reg rs, std::uint32_t val) {
        operand2 op2;
        bool inverse;
        if (val == 0) {
            // Avoid the ALU, may improve pipeline.
            MOV(rd, 0);
            return true;
        } else if (try_make_operand2_allow_inverse(val, op2, &inverse)) {
            if (!inverse) {
                AND(rd, rs, op2);
            } else {
                BIC(rd, rs, op2);
            }
            return true;
        } else {
            if (context_info.bARMv7) {
                // Check if we have a single pattern of sequential bits.
                int seq = -1;
                for (int i = 0; i < 32; ++i) {
                    if (((val >> i) & 1) == 0) {
                        if (seq == -1) {
                            // The width is all bits previous to this, set to 1.
                            seq = i;
                        }
                    } else if (seq != -1) {
                        // Uh oh, more than one sequence.
                        seq = -2;
                    }
                }

                if (seq > 0) {
                    UBFX(rd, rs, 0, seq);
                    return true;
                }
            }

            int ops = 0;
            for (int i = 0; i < 32; i += 2) {
                std::uint8_t bits = rotr(val, i) & 0xFF;
                // If either low bit is not set, we need to use a BIC for them.
                if ((bits & 3) != 3) {
                    ++ops;
                    i += 8 - 2;
                }
            }

            // The worst case is 4 (e.g. 0x55555555.)
            if (ops > 3 && context_info.bARMv7) {
                return false;
            }

            bool first = true;
            for (int i = 0; i < 32; i += 2) {
                std::uint8_t bits = rotr(val, i) & 0xFF;
                if ((bits & 3) != 3) {
                    std::uint8_t rotation = i == 0 ? 0 : 16 - i / 2;
                    if (first) {
                        BIC(rd, rs, operand2(~bits, rotation));
                        first = false;
                    } else {
                        BIC(rd, rd, operand2(~bits, rotation));
                    }
                    // Well, we took care of these other bits while we were at it.
                    i += 8 - 2;
                }
            }
            return true;
        }
    }

    void armx_emitter::CMPI2R(arm_reg rs, std::uint32_t val, arm_reg scratch) {
        if (!TryCMPI2R(rs, val)) {
            MOVI2R(scratch, val);
            CMP(rs, scratch);
        }
    }

    bool armx_emitter::TryCMPI2R(arm_reg rs, std::uint32_t val) {
        operand2 op2;
        bool negated;
        if (try_make_operand2_allow_negation(val, op2, &negated)) {
            if (!negated)
                CMP(rs, op2);
            else
                CMN(rs, op2);
            return true;
        } else {
            return false;
        }
    }

    void armx_emitter::TSTI2R(arm_reg rs, std::uint32_t val, arm_reg scratch) {
        if (!TryTSTI2R(rs, val)) {
            MOVI2R(scratch, val);
            TST(rs, scratch);
        }
    }

    bool armx_emitter::TryTSTI2R(arm_reg rs, std::uint32_t val) {
        operand2 op2;
        if (try_make_operand2(val, op2)) {
            TST(rs, op2);
            return true;
        } else {
            return false;
        }
    }

    void armx_emitter::ORI2R(arm_reg rd, arm_reg rs, std::uint32_t val, arm_reg scratch) {
        if (!TryORI2R(rd, rs, val)) {
            MOVI2R(scratch, val);
            ORR(rd, rs, scratch);
        }
    }

    bool armx_emitter::TryORI2R(arm_reg rd, arm_reg rs, std::uint32_t val) {
        operand2 op2;
        if (val == 0) {
            // Avoid the ALU, may improve pipeline.
            if (rd != rs) {
                MOV(rd, rs);
            }
            return true;
        } else if (try_make_operand2(val, op2)) {
            ORR(rd, rs, op2);
            return true;
        } else {
            int ops = 0;
            for (int i = 0; i < 32; i += 2) {
                std::uint8_t bits = rotr(val, i) & 0xFF;
                // If either low bit is set, we need to use a ORR for them.
                if ((bits & 3) != 0) {
                    ++ops;
                    i += 8 - 2;
                }
            }

            // The worst case is 4 (e.g. 0x55555555.)  But MVN can make it 2.  Not sure if better.
            bool inversed;
            if (try_make_operand2_allow_inverse(val, op2, &inversed) && ops >= 3) {
                return false;
            } else if (ops > 3 && context_info.bARMv7) {
                return false;
            }

            bool first = true;
            for (int i = 0; i < 32; i += 2) {
                std::uint8_t bits = rotr(val, i) & 0xFF;
                if ((bits & 3) != 0) {
                    std::uint8_t rotation = i == 0 ? 0 : 16 - i / 2;
                    if (first) {
                        ORR(rd, rs, operand2(bits, rotation));
                        first = false;
                    } else {
                        ORR(rd, rd, operand2(bits, rotation));
                    }
                    // Well, we took care of these other bits while we were at it.
                    i += 8 - 2;
                }
            }
            return true;
        }
    }

    void armx_emitter::EORI2R(arm_reg rd, arm_reg rs, std::uint32_t val, arm_reg scratch) {
        if (!TryEORI2R(rd, rs, val)) {
            MOVI2R(scratch, val);
            EOR(rd, rs, scratch);
        }
    }

    bool armx_emitter::TryEORI2R(arm_reg rd, arm_reg rs, std::uint32_t val) {
        operand2 op2;
        if (val == 0) {
            if (rd != rs) {
                MOV(rd, rs);
            }
            return true;
        } else if (try_make_operand2(val, op2)) {
            EOR(rd, rs, op2);
            return true;
        } else {
            return false;
        }
    }

    void armx_emitter::flush_lit_pool() {
        for (literal_pool &pool : current_lit_pool) {
            // Search for duplicates
            for (literal_pool &old_pool : current_lit_pool) {
                if (old_pool.val == pool.val)
                    pool.loc = old_pool.loc;
            }

            // Write the constant to Literal Pool
            if (!pool.loc) {
                pool.loc = reinterpret_cast<intptr_t>(code);
                write32(pool.val);
            }

            std::int32_t offset = static_cast<std::uint32_t>(pool.loc - reinterpret_cast<intptr_t>(pool.ldr_address) - 8);

            // Backpatch the LDR
            *reinterpret_cast<std::uint32_t *>(pool.ldr_address) |= (offset >= 0) << 23 | abs(offset);
        }

        // TODO: Save a copy of previous pools in case they are still in range.
        current_lit_pool.clear();
    }

    void armx_emitter::add_new_lit(std::uint32_t val) {
        literal_pool pool_item;
        pool_item.loc = 0;
        pool_item.val = val;
        pool_item.ldr_address = code;
        current_lit_pool.push_back(pool_item);
    }

    void armx_emitter::MOVI2R(arm_reg reg, std::uint32_t val, bool optimize) {
        operand2 op2;
        bool inverse;

        // Unused
        if (!optimize && context_info.bARMv7) {
            // For backpatching on ARMv7
            MOVW(reg, val & 0xFFFF);
            MOVT(reg, val, true);
            return;
        }

        if (try_make_operand2_allow_inverse(val, op2, &inverse)) {
            inverse ? MVN(reg, op2) : MOV(reg, op2);
        } else {
            if (context_info.bARMv7) {
                // Use MOVW+MOVT for ARMv7+
                MOVW(reg, val & 0xFFFF);
                if (val & 0xFFFF0000)
                    MOVT(reg, val, true);
            } else {
                // Use literal pool for ARMv6.
                add_new_lit(val);
                LDRLIT(reg, 0, 0); // To be backpatched later
            }
        }
    }

    static const char *armRegStrings[] = {
        "r0",
        "r1",
        "r2",
        "r3",
        "r4",
        "r5",
        "r6",
        "r7",
        "r8",
        "r9",
        "r10",
        "r11",
        "r12",
        "r13",
        "r14",
        "PC",

        "s0",
        "s1",
        "s2",
        "s3",
        "s4",
        "s5",
        "s6",
        "s7",
        "s8",
        "s9",
        "s10",
        "s11",
        "s12",
        "s13",
        "s14",
        "s15",

        "std::int16_t",
        "s17",
        "s18",
        "s19",
        "s20",
        "s21",
        "s22",
        "s23",
        "s24",
        "s25",
        "s26",
        "s27",
        "s28",
        "s29",
        "s30",
        "s31",

        "d0",
        "d1",
        "d2",
        "d3",
        "d4",
        "d5",
        "d6",
        "d7",
        "d8",
        "d9",
        "d10",
        "d11",
        "d12",
        "d13",
        "d14",
        "d15",

        "d16",
        "d17",
        "d18",
        "d19",
        "d20",
        "d21",
        "d22",
        "d23",
        "d24",
        "d25",
        "d26",
        "d27",
        "d28",
        "d29",
        "d30",
        "d31",

        "q0",
        "q1",
        "q2",
        "q3",
        "q4",
        "q5",
        "q6",
        "q7",
        "q8",
        "q9",
        "q10",
        "q11",
        "q12",
        "q13",
        "q14",
        "q15",
    };

    const char *arm_reg_as_string(arm_reg reg) {
        if ((unsigned int)reg >= sizeof(armRegStrings) / sizeof(armRegStrings[0]))
            return "(bad)";
        return armRegStrings[(int)reg];
    }

    void armx_emitter::quick_call_function(arm_reg reg, const void *func) {
        if (BLInRange(func)) {
            BL(func);
        } else {
            MOVP2R(reg, func);
            BL(reg);
        }
    }

    void armx_emitter::set_code_pointer(std::uint8_t *ptr) {
        code = ptr;
        startcode = code;
        last_cache_flush_end = ptr;
    }

    const std::uint8_t *armx_emitter::get_code_pointer() const {
        return code;
    }

    std::uint8_t *armx_emitter::get_writeable_code_ptr() {
        return code;
    }

    void armx_emitter::reserve_codespace(std::uint32_t bytes) {
        for (std::uint32_t i = 0; i < bytes / 4; i++)
            write32(0xE1200070); //bkpt 0
    }

    const std::uint8_t *armx_emitter::align_code16() {
        reserve_codespace((-(intptr_t)code) & 15);
        return code;
    }

    const std::uint8_t *armx_emitter::align_code_page() {
        reserve_codespace((-(intptr_t)code) & 4095);
        return code;
    }

    void armx_emitter::flush_icache() {
        flush_icache_section(last_cache_flush_end, code);
        last_cache_flush_end = code;
    }

    void armx_emitter::flush_icache_section(std::uint8_t *start, std::uint8_t *end) {
#if EKA2L1_PLATFORM(IOS)
        // Header file says this is equivalent to: sys_icache_invalidate(start, end - start);
        sys_cache_control(kCacheFunctionPrepareForExecution, start, end - start);
#elif EKA2L1_PLATFORM(WIN32)
        FlushInstructionCache(GetCurrentProcess(), start, end - start);
#elif EKA2L1_ARCH(ARM)

#if defined(__clang__) || defined(__ANDROID__)
        __clear_cache(start, end);
#else
        __builtin___clear_cache(start, end);
#endif

#endif
    }

    void armx_emitter::set_cc(cc_flags cond) {
        condition = cond << 28;
    }

    void armx_emitter::NOP(int count) {
        for (int i = 0; i < count; i++) {
            write32(condition | 0x01A00000);
        }
    }

    void armx_emitter::SETEND(bool BE) {
        //SETEND is non-conditional
        write32(0xF1010000 | (BE << 9));
    }
    void armx_emitter::BKPT(std::uint16_t arg) {
        write32(condition | 0x01200070 | (arg << 4 & 0x000FFF00) | (arg & 0x0000000F));
    }
    void armx_emitter::YIELD() {
        write32(condition | 0x0320F001);
    }

    fixup_branch armx_emitter::B() {
        fixup_branch branch;
        branch.type = 0; // Zero for B
        branch.ptr = code;
        branch.condition = condition;
        //We'll write NOP here for now.
        write32(condition | 0x01A00000);
        return branch;
    }
    fixup_branch armx_emitter::BL() {
        fixup_branch branch;
        branch.type = 1; // Zero for B

        branch.ptr = code;
        branch.condition = condition;
        //We'll write NOP here for now.
        write32(condition | 0x01A00000);
        return branch;
    }

    fixup_branch armx_emitter::B_CC(cc_flags Cond) {
        fixup_branch branch;
        branch.type = 0; // Zero for B
        branch.ptr = code;
        branch.condition = Cond << 28;
        //We'll write NOP here for now.
        write32(condition | 0x01A00000);
        return branch;
    }

    void armx_emitter::B_CC(cc_flags Cond, const void *fnptr) {
        ptrdiff_t distance = (intptr_t)fnptr - ((intptr_t)(code) + 8);
        LOG_ERROR_IF(COMMON, (distance <= -0x2000000) || (distance >= 0x2000000), "B_CC out of range ({} calls {})", fmt::ptr(code), fmt::ptr(fnptr));
        write32((Cond << 28) | 0x0A000000 | ((distance >> 2) & 0x00FFFFFF));
    }

    fixup_branch armx_emitter::BL_CC(cc_flags Cond) {
        fixup_branch branch;
        branch.type = 1; // Zero for B
        branch.ptr = code;
        branch.condition = Cond << 28;
        //We'll write NOP here for now.
        write32(condition | 0x01A00000);
        return branch;
    }
    void armx_emitter::set_jump_target(fixup_branch const &branch) {
        ptrdiff_t distance = ((intptr_t)(code)-8) - (intptr_t)branch.ptr;
        LOG_ERROR_IF(COMMON, (distance <= -0x2000000) || (distance >= 0x2000000), "SetJumpTarget out of range ({} calls {})", fmt::ptr(code),
            fmt::ptr(branch.ptr));

        std::uint32_t instr = (std::uint32_t)(branch.condition | ((distance >> 2) & 0x00FFFFFF));
        instr |= branch.type == 0 ? /* B */ 0x0A000000 : /* BL */ 0x0B000000;
        *(std::uint32_t *)branch.ptr = instr;
    }
    void armx_emitter::B(const void *fnptr) {
        ptrdiff_t distance = (intptr_t)fnptr - (intptr_t(code) + 8);
        LOG_ERROR_IF(COMMON, (distance <= -0x2000000) || (distance >= 0x2000000), "B out of range ({} calls {})", fmt::ptr(code), fmt::ptr(fnptr));

        write32(condition | 0x0A000000 | ((distance >> 2) & 0x00FFFFFF));
    }

    void armx_emitter::B(arm_reg src) {
        write32(condition | 0x012FFF10 | src);
    }

    bool armx_emitter::BLInRange(const void *fnptr) const {
        ptrdiff_t distance = (intptr_t)fnptr - (intptr_t(code) + 8);

        // If branch to the thumb code, don't use this!!! :o
        if ((distance <= -0x2000000) || (distance >= 0x2000000) || (distance & 1))
            return false;
        else
            return true;
    }

    void armx_emitter::BL(const void *fnptr) {
        ptrdiff_t distance = (intptr_t)fnptr - (intptr_t(code) + 8);
        LOG_ERROR_IF(COMMON, (distance <= -0x2000000) || (distance >= 0x2000000), "BL out of range ({} calls {})", 
            fmt::ptr(code), fmt::ptr(fnptr));
        write32(condition | 0x0B000000 | ((distance >> 2) & 0x00FFFFFF));
    }
    void armx_emitter::BL(arm_reg src) {
        write32(condition | 0x012FFF30 | src);
    }

    void armx_emitter::PUSH(const int num, ...) {
        std::uint16_t RegList = 0;
        std::uint8_t Reg;
        int i;
        va_list vl;
        va_start(vl, num);
        for (i = 0; i < num; i++) {
            Reg = va_arg(vl, std::uint32_t);
            RegList |= (1 << Reg);
        }
        va_end(vl);
        write32(condition | (2349 << 16) | RegList);
    }

    void armx_emitter::POP(const int num, ...) {
        std::uint16_t RegList = 0;
        std::uint8_t Reg;
        int i;
        va_list vl;
        va_start(vl, num);
        for (i = 0; i < num; i++) {
            Reg = va_arg(vl, std::uint32_t);
            RegList |= (1 << Reg);
        }
        va_end(vl);
        write32(condition | (2237 << 16) | RegList);
    }

    void armx_emitter::write_shifted_data_op(std::uint32_t op, bool SetFlags, arm_reg dest, arm_reg src, operand2 op2) {
        write32(condition | (13 << 21) | (SetFlags << 20) | (dest << 12) | op2.Imm5() | (op << 4) | src);
    }
    void armx_emitter::write_shifted_data_op(std::uint32_t op, bool SetFlags, arm_reg dest, arm_reg src, arm_reg op2) {
        write32(condition | (13 << 21) | (SetFlags << 20) | (dest << 12) | (op2 << 8) | (op << 4) | src);
    }

    // IMM, REG, IMMSREG, RSR
    // -1 for invalid if the instruction doesn't support that
    const std::int32_t INST_OPS[][4] = {
        { 16, 0, 0, 0 }, // AND(s)
        { 17, 1, 1, 1 }, // EOR(s)
        { 18, 2, 2, 2 }, // SUB(s)
        { 19, 3, 3, 3 }, // RSB(s)
        { 20, 4, 4, 4 }, // ADD(s)
        { 21, 5, 5, 5 }, // ADC(s)
        { 22, 6, 6, 6 }, // SBC(s)
        { 23, 7, 7, 7 }, // RSC(s)
        { 24, 8, 8, 8 }, // TST
        { 25, 9, 9, 9 }, // TEQ
        { 26, 10, 10, 10 }, // CMP
        { 27, 11, 11, 11 }, // CMN
        { 28, 12, 12, 12 }, // ORR(s)
        { 29, 13, 13, 13 }, // MOV(s)
        { 30, 14, 14, 14 }, // BIC(s)
        { 31, 15, 15, 15 }, // MVN(s)
        { 24, -1, -1, -1 }, // MOVW
        { 26, -1, -1, -1 }, // MOVT
    };

    const char *InstNames[] = {
        "AND",
        "EOR",
        "SUB",
        "RSB",
        "ADD",
        "ADC",
        "SBC",
        "RSC",
        "TST",
        "TEQ",
        "CMP",
        "CMN",
        "ORR",
        "MOV",
        "BIC",
        "MVN",
        "MOVW",
        "MOVT",
    };

    void armx_emitter::AND(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(0, Rd, Rn, Rm); }
    void armx_emitter::ANDS(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(0, Rd, Rn, Rm, true); }
    void armx_emitter::EOR(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(1, Rd, Rn, Rm); }
    void armx_emitter::EORS(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(1, Rd, Rn, Rm, true); }
    void armx_emitter::SUB(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(2, Rd, Rn, Rm); }
    void armx_emitter::SUBS(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(2, Rd, Rn, Rm, true); }
    void armx_emitter::RSB(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(3, Rd, Rn, Rm); }
    void armx_emitter::RSBS(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(3, Rd, Rn, Rm, true); }
    void armx_emitter::ADD(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(4, Rd, Rn, Rm); }
    void armx_emitter::ADDS(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(4, Rd, Rn, Rm, true); }
    void armx_emitter::ADC(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(5, Rd, Rn, Rm); }
    void armx_emitter::ADCS(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(5, Rd, Rn, Rm, true); }
    void armx_emitter::SBC(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(6, Rd, Rn, Rm); }
    void armx_emitter::SBCS(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(6, Rd, Rn, Rm, true); }
    void armx_emitter::RSC(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(7, Rd, Rn, Rm); }
    void armx_emitter::RSCS(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(7, Rd, Rn, Rm, true); }
    void armx_emitter::TST(arm_reg Rn, operand2 Rm) { write_instruction(8, R0, Rn, Rm, true); }
    void armx_emitter::TEQ(arm_reg Rn, operand2 Rm) { write_instruction(9, R0, Rn, Rm, true); }
    void armx_emitter::CMP(arm_reg Rn, operand2 Rm) { write_instruction(10, R0, Rn, Rm, true); }
    void armx_emitter::CMN(arm_reg Rn, operand2 Rm) { write_instruction(11, R0, Rn, Rm, true); }
    void armx_emitter::ORR(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(12, Rd, Rn, Rm); }
    void armx_emitter::ORRS(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(12, Rd, Rn, Rm, true); }
    void armx_emitter::MOV(arm_reg Rd, operand2 Rm) { write_instruction(13, Rd, R0, Rm); }
    void armx_emitter::MOVS(arm_reg Rd, operand2 Rm) { write_instruction(13, Rd, R0, Rm, true); }
    void armx_emitter::BIC(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(14, Rd, Rn, Rm); }
    void armx_emitter::BICS(arm_reg Rd, arm_reg Rn, operand2 Rm) { write_instruction(14, Rd, Rn, Rm, true); }
    void armx_emitter::MVN(arm_reg Rd, operand2 Rm) { write_instruction(15, Rd, R0, Rm); }
    void armx_emitter::MVNS(arm_reg Rd, operand2 Rm) { write_instruction(15, Rd, R0, Rm, true); }
    void armx_emitter::MOVW(arm_reg Rd, operand2 Rm) { write_instruction(16, Rd, R0, Rm); }
    void armx_emitter::MOVT(arm_reg Rd, operand2 Rm, bool TopBits) { write_instruction(17, Rd, R0, TopBits ? Rm.value_ >> 16 : Rm); }

    void armx_emitter::write_instruction(std::uint32_t Op, arm_reg Rd, arm_reg Rn, operand2 Rm, bool SetFlags) // This can get renamed later
    {
        std::int32_t op = INST_OPS[Op][Rm.get_type()]; // Type always decided by last operand
        std::uint32_t Data = Rm.get_data();
        if (Rm.get_type() == TYPE_IMM) {
            switch (Op) {
            // MOV cases that support IMM16
            case 16:
            case 17:
                Data = Rm.Imm16();
                break;
            default:
                break;
            }
        }
        if (op == -1)
            assert((false, "%s not yet support %d", InstNames[Op]) && Rm.get_type());
        write32(condition | (op << 21) | (SetFlags ? (1 << 20) : 0) | Rn << 16 | Rd << 12 | Data);
    }

    // Data Operations
    void armx_emitter::write_signed_multiply(std::uint32_t Op, std::uint32_t Op2, std::uint32_t Op3, arm_reg dest, arm_reg r1, arm_reg r2) {
        write32(condition | (0x7 << 24) | (Op << 20) | (dest << 16) | (Op2 << 12) | (r1 << 8) | (Op3 << 5) | (1 << 4) | r2);
    }
    void armx_emitter::UDIV(arm_reg dest, arm_reg dividend, arm_reg divisor) {
        if (!context_info.bIDIVa)
            LOG_ERROR(COMMON, "Trying to use integer divide on hardware that doesn't support it. Bad programmer.");
        write_signed_multiply(3, 0xF, 0, dest, divisor, dividend);
    }
    void armx_emitter::SDIV(arm_reg dest, arm_reg dividend, arm_reg divisor) {
        if (!context_info.bIDIVa)
            LOG_ERROR(COMMON, "Trying to use integer divide on hardware that doesn't support it. Bad programmer.");
        write_signed_multiply(1, 0xF, 0, dest, divisor, dividend);
    }

    void armx_emitter::LSL(arm_reg dest, arm_reg src, operand2 op2) { write_shifted_data_op(0, false, dest, src, op2); }
    void armx_emitter::LSLS(arm_reg dest, arm_reg src, operand2 op2) { write_shifted_data_op(0, true, dest, src, op2); }
    void armx_emitter::LSL(arm_reg dest, arm_reg src, arm_reg op2) { write_shifted_data_op(1, false, dest, src, op2); }
    void armx_emitter::LSLS(arm_reg dest, arm_reg src, arm_reg op2) { write_shifted_data_op(1, true, dest, src, op2); }
    void armx_emitter::LSR(arm_reg dest, arm_reg src, operand2 op2) {
        assert((op2.get_type() != TYPE_IMM || op2.Imm5() != 0) && "LSR must have a non-zero shift (use LSL.)");
        write_shifted_data_op(2, false, dest, src, op2);
    }
    void armx_emitter::LSRS(arm_reg dest, arm_reg src, operand2 op2) {
        assert((op2.get_type() != TYPE_IMM || op2.Imm5() != 0) && "LSRS must have a non-zero shift (use LSLS.)");
        write_shifted_data_op(2, true, dest, src, op2);
    }
    void armx_emitter::LSR(arm_reg dest, arm_reg src, arm_reg op2) { write_shifted_data_op(3, false, dest, src, op2); }
    void armx_emitter::LSRS(arm_reg dest, arm_reg src, arm_reg op2) { write_shifted_data_op(3, true, dest, src, op2); }
    void armx_emitter::ASR(arm_reg dest, arm_reg src, operand2 op2) {
        assert((op2.get_type() != TYPE_IMM || op2.Imm5() != 0) && "ASR must have a non-zero shift (use LSL.)");
        write_shifted_data_op(4, false, dest, src, op2);
    }
    void armx_emitter::ASRS(arm_reg dest, arm_reg src, operand2 op2) {
        assert((op2.get_type() != TYPE_IMM || op2.Imm5() != 0) && "ASRS must have a non-zero shift (use LSLS.)");
        write_shifted_data_op(4, true, dest, src, op2);
    }
    void armx_emitter::ASR(arm_reg dest, arm_reg src, arm_reg op2) { write_shifted_data_op(5, false, dest, src, op2); }
    void armx_emitter::ASRS(arm_reg dest, arm_reg src, arm_reg op2) { write_shifted_data_op(5, true, dest, src, op2); }

    void armx_emitter::MUL(arm_reg dest, arm_reg src, arm_reg op2) {
        write32(condition | (dest << 16) | (src << 8) | (9 << 4) | op2);
    }

    void armx_emitter::MULS(arm_reg dest, arm_reg src, arm_reg op2) {
        write32(condition | (1 << 20) | (dest << 16) | (src << 8) | (9 << 4) | op2);
    }

    void armx_emitter::MLA(arm_reg dest, arm_reg src1, arm_reg src2, arm_reg add) {
        write32(condition | (1 << 21) | (dest << 16) | (add << 12) | (src2 << 8) | (9 << 4) | src1);
    }

    void armx_emitter::MLAS(arm_reg dest, arm_reg src1, arm_reg src2, arm_reg add) {
        write32(condition | (0b11 << 20) | (dest << 16) | (add << 12) | (src2 << 8) | (9 << 4) | src1);
    }

    void armx_emitter::write_4op_multiply(std::uint32_t op, arm_reg destLo, arm_reg destHi, arm_reg rm, arm_reg rn) {
        write32(condition | (op << 20) | (destHi << 16) | (destLo << 12) | (rm << 8) | (9 << 4) | rn);
    }

    void armx_emitter::UMULL(arm_reg destLo, arm_reg destHi, arm_reg rm, arm_reg rn) {
        write_4op_multiply(0x8, destLo, destHi, rn, rm);
    }

    void armx_emitter::UMULLS(arm_reg destLo, arm_reg destHi, arm_reg rm, arm_reg rn) {
        write_4op_multiply(0x9, destLo, destHi, rn, rm);
    }

    void armx_emitter::SMULL(arm_reg destLo, arm_reg destHi, arm_reg rm, arm_reg rn) {
        write_4op_multiply(0xC, destLo, destHi, rn, rm);
    }

    void armx_emitter::SMULLS(arm_reg destLo, arm_reg destHi, arm_reg rm, arm_reg rn) {
        write_4op_multiply(0xD, destLo, destHi, rn, rm);
    }

    void armx_emitter::UMLAL(arm_reg destLo, arm_reg destHi, arm_reg rm, arm_reg rn) {
        write_4op_multiply(0xA, destLo, destHi, rn, rm);
    }

    void armx_emitter::UMLALS(arm_reg destLo, arm_reg destHi, arm_reg rm, arm_reg rn) {
        write_4op_multiply(0xB, destLo, destHi, rn, rm);
    }

    void armx_emitter::SMLAL(arm_reg destLo, arm_reg destHi, arm_reg rm, arm_reg rn) {
        write_4op_multiply(0xE, destLo, destHi, rn, rm);
    }

    void armx_emitter::SMLALS(arm_reg destLo, arm_reg destHi, arm_reg rm, arm_reg rn) {
        write_4op_multiply(0xF, destLo, destHi, rn, rm);
    }

    void armx_emitter::SMULxy(arm_reg dest, arm_reg rn, arm_reg rm, bool n, bool m) {
        write32(condition | (0x16 << 20) | (dest << 16) | (rm << 8) | (1 << 7) | (m << 6) | (n << 5) | rn);
    }

    void armx_emitter::SMLAxy(arm_reg dest, arm_reg rn, arm_reg rm, arm_reg ra, bool n, bool m) {
        write32(condition | (0x10 << 20) | (dest << 16) | (ra << 12) | (rm << 8) | (1 << 7) | (m << 6) | (n << 5) | rn);
    }

    void armx_emitter::SMULWy(arm_reg dest, arm_reg rn, arm_reg rm, bool top) {
        write32(condition | (0x12 << 20) | (dest << 16) | (rm << 8) | (1 << 7) | (top << 6) | 0x20 | rn);
    }

    void armx_emitter::SMLAWy(arm_reg dest, arm_reg rn, arm_reg rm, arm_reg ra, bool top) {
        write32(condition | (0x12 << 20) | (dest << 16) | (ra << 12) | (rm << 8) | (1 << 7) | (top << 6) | rn);
    }

    void armx_emitter::UBFX(arm_reg dest, arm_reg rn, std::uint8_t lsb, std::uint8_t width) {
        write32(condition | (0x7E0 << 16) | ((width - 1) << 16) | (dest << 12) | (lsb << 7) | (5 << 4) | rn);
    }

    void armx_emitter::SBFX(arm_reg dest, arm_reg rn, std::uint8_t lsb, std::uint8_t width) {
        write32(condition | (0x7A0 << 16) | ((width - 1) << 16) | (dest << 12) | (lsb << 7) | (5 << 4) | rn);
    }

    void armx_emitter::CLZ(arm_reg rd, arm_reg rm) {
        write32(condition | (0x16F << 16) | (rd << 12) | (0xF1 << 4) | rm);
    }

    void armx_emitter::PLD(arm_reg rn, int offset, bool forWrite) {
        LOG_ERROR_IF(COMMON, !(offset < 0x3ff && offset > -0x3ff), "PLD: Max 12 bits of offset allowed");

        bool U = offset >= 0;
        if (offset < 0)
            offset = -offset;
        bool R = !forWrite;
        // Conditions not allowed
        write32((0xF5 << 24) | (U << 23) | (R << 22) | (1 << 20) | ((int)rn << 16) | (0xF << 12) | offset);
    }

    void armx_emitter::SEL(arm_reg rd, arm_reg rn, arm_reg rm) {
        write32(condition | (0x68 << 20) | (rn << 16) | (rd << 12) | (0xFB << 4) | (rm));
    }

    void armx_emitter::PKHBT(arm_reg rd, arm_reg rn, arm_reg rm, const int left_shift) {
        if (left_shift > 31) {
            LOG_ERROR(COMMON, "Left shift is too big!");
        }

        write32(condition | (0x68 << 20) | (rn << 16) | (rd << 12) | ((left_shift & 0b11111) << 7) | (0x1 << 4) | (rm));
    }

    void armx_emitter::PKHTB(arm_reg rd, arm_reg rn, arm_reg rm, const int right_shift_with_32_equals_0) {
        if (right_shift_with_32_equals_0 > 31) {
            LOG_ERROR(COMMON, "Right shift is too big!");
        }

        write32(condition | (0x68 << 20) | (rn << 16) | (rd << 12) | ((right_shift_with_32_equals_0 & 0b11111) << 7) | (0x5 << 4) | (rm));
    }

    void armx_emitter::BFI(arm_reg rd, arm_reg rn, std::uint8_t lsb, std::uint8_t width) {
        std::uint32_t msb = (lsb + width - 1);
        if (msb > 31)
            msb = 31;
        write32(condition | (0x7C0 << 16) | (msb << 16) | (rd << 12) | (lsb << 7) | (1 << 4) | rn);
    }

    void armx_emitter::BFC(arm_reg rd, std::uint8_t lsb, std::uint8_t width) {
        std::uint32_t msb = (lsb + width - 1);
        if (msb > 31)
            msb = 31;
        write32(condition | (0x7C0 << 16) | (msb << 16) | (rd << 12) | (lsb << 7) | (1 << 4) | 15);
    }

    void armx_emitter::UXTB(arm_reg dest, arm_reg op2, std::uint8_t rotation_base_8) {
        write32(condition | (0x6EF << 16) | (dest << 12) | ((rotation_base_8 & 0b11) << 10) | (7 << 4) | op2);
    }

    void armx_emitter::UXTH(arm_reg dest, arm_reg op2, std::uint8_t rotation_base_8) {
        write32(condition | (0x6FF << 16) | (dest << 12) | ((rotation_base_8 & 0b11) << 10) | (7 << 4) | op2);
    }

    void armx_emitter::SXTB(arm_reg dest, arm_reg op2, std::uint8_t rotation_base_8) {
        write32(condition | (0x6AF << 16) | (dest << 12) | ((rotation_base_8 & 0b11) << 10) | (7 << 4) | op2);
    }

    void armx_emitter::SXTH(arm_reg dest, arm_reg op2, std::uint8_t rotation) {
        SXTAH(dest, (arm_reg)15, op2, rotation);
    }
    void armx_emitter::SXTAH(arm_reg dest, arm_reg add, arm_reg src, std::uint8_t rotation) {
        // bits ten and 11 are the rotation amount, see 8.8.232 for more
        // information
        write32(condition | (0x6B << 20) | (add << 16) | (dest << 12) | (rotation << 10) | (7 << 4) | src);
    }

    void armx_emitter::UXTAH(arm_reg dest, arm_reg add, arm_reg src, std::uint8_t rotation) {
        write32(condition | (0x6F << 20) | (add << 16) | (dest << 12) | (rotation << 10) | (7 << 4) | src);
    }

    void armx_emitter::SXTAB(arm_reg dest, arm_reg add, arm_reg src, std::uint8_t rotation) {
        write32(condition | (0x6A << 20) | (add << 16) | (dest << 12) | (rotation << 10) | (7 << 4) | src);
    }

    void armx_emitter::UXTAB(arm_reg dest, arm_reg add, arm_reg src, std::uint8_t rotation) {
        write32(condition | (0x6E << 20) | (add << 16) | (dest << 12) | (rotation << 10) | (7 << 4) | src);
    }

    void armx_emitter::RBIT(arm_reg dest, arm_reg src) {
        write32(condition | (0x6F << 20) | (0xF << 16) | (dest << 12) | (0xF3 << 4) | src);
    }
    void armx_emitter::REV(arm_reg dest, arm_reg src) {
        write32(condition | (0x6BF << 16) | (dest << 12) | (0xF3 << 4) | src);
    }
    void armx_emitter::REV16(arm_reg dest, arm_reg src) {
        write32(condition | (0x6BF << 16) | (dest << 12) | (0xFB << 4) | src);
    }

    void armx_emitter::_MSR(bool write_nzcvq, bool write_g, operand2 op2) {
        write32(condition | (0x320F << 12) | (write_nzcvq << 19) | (write_g << 18) | op2.Imm12Mod());
    }
    void armx_emitter::_MSR(bool write_nzcvq, bool write_g, arm_reg src) {
        write32(condition | (0x120F << 12) | (write_nzcvq << 19) | (write_g << 18) | src);
    }
    void armx_emitter::MRS(arm_reg dest) {
        write32(condition | (16 << 20) | (15 << 16) | (dest << 12));
    }
    void armx_emitter::LDREX(arm_reg dest, arm_reg base) {
        write32(condition | (25 << 20) | (base << 16) | (dest << 12) | 0xF9F);
    }
    void armx_emitter::STREX(arm_reg result, arm_reg base, arm_reg op) {
        assert(((result != base && result != op)) && "STREX dest can't be other two registers");
        write32(condition | (24 << 20) | (base << 16) | (result << 12) | (0xF9 << 4) | op);
    }
    void armx_emitter::DMB() {
        write32(0xF57FF05E);
    }
    void armx_emitter::SVC(operand2 op) {
        write32(condition | (0x0F << 24) | op.Imm24());
    }

    void armx_emitter::LDRLIT(arm_reg dest, std::uint32_t offset, bool Add) {
        write32(condition | 0x05 << 24 | Add << 23 | 0x1F << 16 | dest << 12 | offset);
    }

    // IMM, REG, IMMSREG, RSR
    // -1 for invalid if the instruction doesn't support that
    const std::int32_t LoadStoreOps[][4] = {
        { 0x40, 0x60, 0x60, -1 }, // STR
        { 0x41, 0x61, 0x61, -1 }, // LDR
        { 0x44, 0x64, 0x64, -1 }, // STRB
        { 0x45, 0x65, 0x65, -1 }, // LDRB
        // Special encodings
        { 0x4, 0x0, -1, -1 }, // STRH
        { 0x5, 0x1, -1, -1 }, // LDRH
        { 0x5, 0x1, -1, -1 }, // LDRSB
        { 0x5, 0x1, -1, -1 }, // LDRSH
    };
    const char *LoadStoreNames[] = {
        "STR",
        "LDR",
        "STRB",
        "LDRB",
        "STRH",
        "LDRH",
        "LDRSB",
        "LDRSH",
    };

    void armx_emitter::write_store_op(std::uint32_t Op, arm_reg Rt, arm_reg Rn, operand2 Rm, bool RegAdd, bool PreIndex, bool WriteBack) {
        std::int32_t op = LoadStoreOps[Op][Rm.get_type()]; // Type always decided by last operand
        std::uint32_t Data;

        // Qualcomm chipsets get /really/ angry if you don't use index, even if the offset is zero.
        // Some of these encodings require Index at all times anyway. Doesn't really matter.
        // bool Index = op2 != 0 ? true : false;
        bool Index = PreIndex;
        bool Add = false;

        // Special Encoding (misc addressing mode)
        bool SpecialOp = false;
        bool Half = false;
        bool SignedLoad = false;

        if (op == -1)
            LOG_ERROR(COMMON, "{} does not support {}", LoadStoreNames[Op], static_cast<int>(Rm.get_type()));

        switch (Op) {
        case 4: // STRH
            SpecialOp = true;
            Half = true;
            SignedLoad = false;
            break;
        case 5: // LDRH
            SpecialOp = true;
            Half = true;
            SignedLoad = false;
            break;
        case 6: // LDRSB
            SpecialOp = true;
            Half = false;
            SignedLoad = true;
            break;
        case 7: // LDRSH
            SpecialOp = true;
            Half = true;
            SignedLoad = true;
            break;
        }
        switch (Rm.get_type()) {
        case TYPE_IMM: {
            std::int32_t Temp = (std::int32_t)Rm.value_;
            Data = abs(Temp);
            // The offset is encoded differently on this one.
            if (SpecialOp)
                Data = ((Data & 0xF0) << 4) | (Data & 0xF);
            if (Temp >= 0)
                Add = true;
        } break;
        case TYPE_REG:
            Data = Rm.get_data();
            Add = RegAdd;
            break;
        case TYPE_IMMSREG:
            if (!SpecialOp) {
                Data = Rm.get_data();
                Add = RegAdd;
                break;
            }
            // Intentional fallthrough: TYPE_IMMSREG not supported for misc addressing.
        default:
            // RSR not supported for any of these
            // We already have the warning above
            BKPT(0x2);
            return;
            break;
        }
        if (SpecialOp) {
            // Add SpecialOp things
            Data = (0x9 << 4) | (SignedLoad << 6) | (Half << 5) | Data;
        }
        write32(condition | (op << 20) | (Index << 24) | (Add << 23) | (WriteBack << 21) | (Rn << 16) | (Rt << 12) | Data);
    }

    void armx_emitter::LDR(arm_reg dest, arm_reg base, operand2 op2, bool RegAdd, bool PreIndex, bool WriteBack) { write_store_op(1, dest, base, op2, RegAdd, PreIndex, WriteBack); }
    void armx_emitter::LDRB(arm_reg dest, arm_reg base, operand2 op2, bool RegAdd, bool PreIndex, bool WriteBack) { write_store_op(3, dest, base, op2, RegAdd, PreIndex, WriteBack); }
    void armx_emitter::LDRH(arm_reg dest, arm_reg base, operand2 op2, bool RegAdd, bool PreIndex, bool WriteBack) { write_store_op(5, dest, base, op2, RegAdd, PreIndex, WriteBack); }
    void armx_emitter::LDRSB(arm_reg dest, arm_reg base, operand2 op2, bool RegAdd, bool PreIndex, bool WriteBack) { write_store_op(6, dest, base, op2, RegAdd, PreIndex, WriteBack); }
    void armx_emitter::LDRSH(arm_reg dest, arm_reg base, operand2 op2, bool RegAdd, bool PreIndex, bool WriteBack) { write_store_op(7, dest, base, op2, RegAdd, PreIndex, WriteBack); }
    void armx_emitter::STR(arm_reg result, arm_reg base, operand2 op2, bool RegAdd, bool PreIndex, bool WriteBack) { write_store_op(0, result, base, op2, RegAdd, PreIndex, WriteBack); }
    void armx_emitter::STRH(arm_reg result, arm_reg base, operand2 op2, bool RegAdd, bool PreIndex, bool WriteBack) { write_store_op(4, result, base, op2, RegAdd, PreIndex, WriteBack); }
    void armx_emitter::STRB(arm_reg result, arm_reg base, operand2 op2, bool RegAdd, bool PreIndex, bool WriteBack) { write_store_op(2, result, base, op2, RegAdd, PreIndex, WriteBack); }

#define VA_TO_REGLIST(RegList, Regnum)       \
    {                                        \
        std::uint8_t Reg;                    \
        va_list vl;                          \
        va_start(vl, Regnum);                \
        for (int i = 0; i < Regnum; i++) {   \
            Reg = va_arg(vl, std::uint32_t); \
            RegList |= (1 << Reg);           \
        }                                    \
        va_end(vl);                          \
    }

    void armx_emitter::write_reg_store_op(std::uint32_t op, arm_reg dest, bool WriteBack, std::uint16_t RegList) {
        write32(condition | (op << 20) | (WriteBack << 21) | (dest << 16) | RegList);
    }
    void armx_emitter::write_vreg_store_op(std::uint32_t op, arm_reg Rn, bool Double, bool WriteBack, arm_reg Vd, std::uint8_t numregs) {
        LOG_ERROR_IF(COMMON, !(!WriteBack || Rn != R_PC), "VLDM/VSTM cannot use WriteBack with PC (PC is deprecated anyway.)");
        write32(condition | (op << 20) | (WriteBack << 21) | (Rn << 16) | encode_vd(Vd) | ((0xA | (int)Double) << 8) | (numregs << (int)Double));
    }
    void armx_emitter::STMFD(arm_reg dest, bool WriteBack, const int Regnum, ...) {
        std::uint16_t RegList = 0;
        VA_TO_REGLIST(RegList, Regnum);
        write_reg_store_op(0x80 | 0x10 | 0, dest, WriteBack, RegList);
    }
    void armx_emitter::LDMFD(arm_reg dest, bool WriteBack, const int Regnum, ...) {
        std::uint16_t RegList = 0;
        VA_TO_REGLIST(RegList, Regnum);
        write_reg_store_op(0x80 | 0x08 | 1, dest, WriteBack, RegList);
    }
    void armx_emitter::STMIA(arm_reg dest, bool WriteBack, const int Regnum, ...) {
        std::uint16_t RegList = 0;
        VA_TO_REGLIST(RegList, Regnum);
        write_reg_store_op(0x80 | 0x08 | 0, dest, WriteBack, RegList);
    }
    void armx_emitter::LDMIA(arm_reg dest, bool WriteBack, const int Regnum, ...) {
        std::uint16_t RegList = 0;
        VA_TO_REGLIST(RegList, Regnum);
        write_reg_store_op(0x80 | 0x08 | 1, dest, WriteBack, RegList);
    }
    void armx_emitter::STM(arm_reg dest, bool Add, bool Before, bool WriteBack, const int Regnum, ...) {
        std::uint16_t RegList = 0;
        VA_TO_REGLIST(RegList, Regnum);
        write_reg_store_op(0x80 | (Before << 4) | (Add << 3) | 0, dest, WriteBack, RegList);
    }
    void armx_emitter::LDM(arm_reg dest, bool Add, bool Before, bool WriteBack, const int Regnum, ...) {
        std::uint16_t RegList = 0;
        VA_TO_REGLIST(RegList, Regnum);
        write_reg_store_op(0x80 | (Before << 4) | (Add << 3) | 1, dest, WriteBack, RegList);
    }

    void armx_emitter::STMBitmask(arm_reg dest, bool Add, bool Before, bool WriteBack, const std::uint16_t RegList) {
        write_reg_store_op(0x80 | (Before << 4) | (Add << 3) | 0, dest, WriteBack, RegList);
    }
    void armx_emitter::LDMBitmask(arm_reg dest, bool Add, bool Before, bool WriteBack, const std::uint16_t RegList) {
        write_reg_store_op(0x80 | (Before << 4) | (Add << 3) | 1, dest, WriteBack, RegList);
    }

    void armx_emitter::QADD(arm_reg rd, arm_reg rm, arm_reg rn) {
        write32(condition | 0x1000000 | (rn << 16) | (rd << 12) | 0x50 | rm);
    }

    void armx_emitter::QSUB(arm_reg rd, arm_reg rm, arm_reg rn) {
        write32(condition | 0x1200000 | (rn << 16) | (rd << 12) | 0x50 | rm);
    }

    void armx_emitter::QDADD(arm_reg rd, arm_reg rm, arm_reg rn) {
        write32(condition | 0x1400000 | (rn << 16) | (rd << 12) | 0x50 | rm);
    }

    void armx_emitter::QDSUB(arm_reg rd, arm_reg rm, arm_reg rn) {
        write32(condition | 0x1600000 | (rn << 16) | (rd << 12) | 0x50 | rm);
    }

#undef VA_TO_REGLIST

    // NEON Specific
    void armx_emitter::VABD(integer_size Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        assert((Vd >= D0) && "Pass invalid register to VABD(float)");
        assert((context_info.bNEON) && "Can't use VABD(float) when CPU doesn't support it");
        bool register_quad = Vd >= Q0;

        // Gets encoded as a double register
        Vd = subbase(Vd);
        Vn = subbase(Vn);
        Vm = subbase(Vm);

        write32((0xF3 << 24) | ((Vd & 0x10) << 18) | (Size << 20) | ((Vn & 0xF) << 16)
            | ((Vd & 0xF) << 12) | (0xD << 8) | ((Vn & 0x10) << 3) | (register_quad << 6)
            | ((Vm & 0x10) << 2) | (Vm & 0xF));
    }
    void armx_emitter::VADD(integer_size Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        assert((Vd >= D0) && "Pass invalid register to VADD(integer)");
        assert((context_info.bNEON) && "Can't use VADD(integer) when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        // Gets encoded as a double register
        Vd = subbase(Vd);
        Vn = subbase(Vn);
        Vm = subbase(Vm);

        write32((0xF2 << 24) | ((Vd & 0x10) << 18) | (Size << 20) | ((Vn & 0xF) << 16)
            | ((Vd & 0xF) << 12) | (0x8 << 8) | ((Vn & 0x10) << 3) | (register_quad << 6)
            | ((Vm & 0x10) << 1) | (Vm & 0xF));
    }
    void armx_emitter::VSUB(integer_size Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        assert((Vd >= Q0) && "Pass invalid register to VSUB(integer)");
        assert((context_info.bNEON) && "Can't use VSUB(integer) when CPU doesn't support it");

        // Gets encoded as a double register
        Vd = subbase(Vd);
        Vn = subbase(Vn);
        Vm = subbase(Vm);

        write32((0xF3 << 24) | ((Vd & 0x10) << 18) | (Size << 20) | ((Vn & 0xF) << 16)
            | ((Vd & 0xF) << 12) | (0x8 << 8) | ((Vn & 0x10) << 3) | (1 << 6)
            | ((Vm & 0x10) << 2) | (Vm & 0xF));
    }

    extern const VFPEnc VFPOps[16][2] = {
        { { 0xE0, 0xA0 }, { -1, -1 } }, // 0: VMLA
        { { 0xE1, 0xA4 }, { -1, -1 } }, // 1: VNMLA
        { { 0xE0, 0xA4 }, { -1, -1 } }, // 2: VMLS
        { { 0xE1, 0xA0 }, { -1, -1 } }, // 3: VNMLS
        { { 0xE3, 0xA0 }, { -1, -1 } }, // 4: VADD
        { { 0xE3, 0xA4 }, { -1, -1 } }, // 5: VSUB
        { { 0xE2, 0xA0 }, { -1, -1 } }, // 6: VMUL
        { { 0xE2, 0xA4 }, { -1, -1 } }, // 7: VNMUL
        { { 0xEB, 0xAC }, { -1 /* 0x3B */, -1 /* 0x70 */ } }, // 8: VABS(Vn(0x0) used for encoding)
        { { 0xE8, 0xA0 }, { -1, -1 } }, // 9: VDIV
        { { 0xEB, 0xA4 }, { -1 /* 0x3B */, -1 /* 0x78 */ } }, // 10: VNEG(Vn(0x1) used for encoding)
        { { 0xEB, 0xAC }, { -1, -1 } }, // 11: VSQRT (Vn(0x1) used for encoding)
        { { 0xEB, 0xA4 }, { -1, -1 } }, // 12: VCMP (Vn(0x4 | #0 ? 1 : 0) used for encoding)
        { { 0xEB, 0xAC }, { -1, -1 } }, // 13: VCMPE (Vn(0x4 | #0 ? 1 : 0) used for encoding)
        { { -1, -1 }, { 0x3B, 0x30 } }, // 14: VABSi
    };

    const char *VFPOpNames[16] = {
        "VMLA",
        "VNMLA",
        "VMLS",
        "VNMLS",
        "VADD",
        "VSUB",
        "VMUL",
        "VNMUL",
        "VABS",
        "VDIV",
        "VNEG",
        "VSQRT",
        "VCMP",
        "VCMPE",
        "VABSi",
    };

    std::uint32_t encode_vd(arm_reg Vd) {
        bool quad_reg = Vd >= Q0;
        bool double_reg = Vd >= D0;

        arm_reg Reg = subbase(Vd);

        if (quad_reg)
            return ((Reg & 0x10) << 18) | ((Reg & 0xF) << 12);
        else {
            if (double_reg)
                return ((Reg & 0x10) << 18) | ((Reg & 0xF) << 12);
            else
                return ((Reg & 0x1) << 22) | ((Reg & 0x1E) << 11);
        }
    }
    std::uint32_t EncodeVn(arm_reg Vn) {
        bool quad_reg = Vn >= Q0;
        bool double_reg = Vn >= D0;

        arm_reg Reg = subbase(Vn);
        if (quad_reg)
            return ((Reg & 0xF) << 16) | ((Reg & 0x10) << 3);
        else {
            if (double_reg)
                return ((Reg & 0xF) << 16) | ((Reg & 0x10) << 3);
            else
                return ((Reg & 0x1E) << 15) | ((Reg & 0x1) << 7);
        }
    }
    std::uint32_t EncodeVm(arm_reg Vm) {
        bool quad_reg = Vm >= Q0;
        bool double_reg = Vm >= D0;

        arm_reg Reg = subbase(Vm);

        if (quad_reg)
            return ((Reg & 0x10) << 1) | (Reg & 0xF);
        else {
            if (double_reg)
                return ((Reg & 0x10) << 1) | (Reg & 0xF);
            else
                return ((Reg & 0x1) << 5) | (Reg >> 1);
        }
    }

    std::uint32_t encodedSize(std::uint32_t value) {
        if (value & I_8)
            return 0;
        else if (value & I_16)
            return 1;
        else if ((value & I_32) || (value & F_32))
            return 2;
        else if (value & I_64)
            return 3;
        else
            LOG_ERROR_IF(COMMON, !(false), "Passed invalid size to integer NEON instruction");
        return 0;
    }

    arm_reg subbase(arm_reg Reg) {
        if (Reg >= S0) {
            if (Reg >= D0) {
                if (Reg >= Q0)
                    return (arm_reg)((Reg - Q0) * 2); // Always gets encoded as a double register
                return (arm_reg)(Reg - D0);
            }
            return (arm_reg)(Reg - S0);
        }
        return Reg;
    }

    arm_reg DScalar(arm_reg dreg, int subScalar) {
        int dr = (int)(subbase(dreg)) & 0xF;
        int scalar = ((subScalar << 4) | dr);
        arm_reg ret = (arm_reg)(D0 + scalar);
        // ILOG("Scalar: %i D0: %i AR: %i", scalar, (int)D0, (int)ret);
        return ret;
    }

    // Convert to a DScalar
    arm_reg QScalar(arm_reg qreg, int subScalar) {
        int dr = (int)(subbase(qreg)) & 0xF;
        if (subScalar & 2) {
            dr++;
        }
        int scalar = (((subScalar & 1) << 4) | dr);
        arm_reg ret = (arm_reg)(D0 + scalar);
        return ret;
    }

    void armx_emitter::write_vfp_data_op(std::uint32_t Op, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        bool quad_reg = Vd >= Q0;
        bool double_reg = Vd >= D0 && Vd < Q0;

        VFPEnc enc = VFPOps[Op][quad_reg];
        if (enc.opc1 == -1 && enc.opc2 == -1)
            LOG_ERROR(COMMON, "{} does not support {}", VFPOpNames[Op], quad_reg ? "NEON" : "VFP");

        std::uint32_t VdEnc = encode_vd(Vd);
        std::uint32_t VnEnc = EncodeVn(Vn);
        std::uint32_t VmEnc = EncodeVm(Vm);
        std::uint32_t cond = quad_reg ? (0xF << 28) : condition;

        write32(cond | (enc.opc1 << 20) | VnEnc | VdEnc | (enc.opc2 << 4) | (quad_reg << 6) | (double_reg << 8) | VmEnc);
    }
    void armx_emitter::VMLA(arm_reg Vd, arm_reg Vn, arm_reg Vm) { write_vfp_data_op(0, Vd, Vn, Vm); }
    void armx_emitter::VNMLA(arm_reg Vd, arm_reg Vn, arm_reg Vm) { write_vfp_data_op(1, Vd, Vn, Vm); }
    void armx_emitter::VMLS(arm_reg Vd, arm_reg Vn, arm_reg Vm) { write_vfp_data_op(2, Vd, Vn, Vm); }
    void armx_emitter::VNMLS(arm_reg Vd, arm_reg Vn, arm_reg Vm) { write_vfp_data_op(3, Vd, Vn, Vm); }
    void armx_emitter::VADD(arm_reg Vd, arm_reg Vn, arm_reg Vm) { write_vfp_data_op(4, Vd, Vn, Vm); }
    void armx_emitter::VSUB(arm_reg Vd, arm_reg Vn, arm_reg Vm) { write_vfp_data_op(5, Vd, Vn, Vm); }
    void armx_emitter::VMUL(arm_reg Vd, arm_reg Vn, arm_reg Vm) { write_vfp_data_op(6, Vd, Vn, Vm); }
    void armx_emitter::VNMUL(arm_reg Vd, arm_reg Vn, arm_reg Vm) { write_vfp_data_op(7, Vd, Vn, Vm); }
    void armx_emitter::VABS(arm_reg Vd, arm_reg Vm) { write_vfp_data_op(8, Vd, D0, Vm); }
    void armx_emitter::VDIV(arm_reg Vd, arm_reg Vn, arm_reg Vm) { write_vfp_data_op(9, Vd, Vn, Vm); }
    void armx_emitter::VNEG(arm_reg Vd, arm_reg Vm) { write_vfp_data_op(10, Vd, D1, Vm); }
    void armx_emitter::VSQRT(arm_reg Vd, arm_reg Vm) { write_vfp_data_op(11, Vd, D1, Vm); }
    void armx_emitter::VCMP(arm_reg Vd, arm_reg Vm) { write_vfp_data_op(12, Vd, D4, Vm); }
    void armx_emitter::VCMPE(arm_reg Vd, arm_reg Vm) { write_vfp_data_op(13, Vd, D4, Vm); }
    void armx_emitter::VCMP(arm_reg Vd) { write_vfp_data_op(12, Vd, D5, D0); }
    void armx_emitter::VCMPE(arm_reg Vd) { write_vfp_data_op(13, Vd, D5, D0); }

    void armx_emitter::VLDMIA(arm_reg ptr, bool WriteBack, arm_reg firstvreg, int numvregs) {
        write_vreg_store_op(0x80 | 0x40 | 0x8 | 1, ptr, firstvreg >= D0, WriteBack, firstvreg, numvregs);
    }

    void armx_emitter::VSTMIA(arm_reg ptr, bool WriteBack, arm_reg firstvreg, int numvregs) {
        write_vreg_store_op(0x80 | 0x40 | 0x8, ptr, firstvreg >= D0, WriteBack, firstvreg, numvregs);
    }

    void armx_emitter::VLDMDB(arm_reg ptr, bool WriteBack, arm_reg firstvreg, int numvregs) {
        LOG_ERROR_IF(COMMON, !(WriteBack), "Writeback is required for VLDMDB");
        write_vreg_store_op(0x80 | 0x040 | 0x10 | 1, ptr, firstvreg >= D0, WriteBack, firstvreg, numvregs);
    }

    void armx_emitter::VSTMDB(arm_reg ptr, bool WriteBack, arm_reg firstvreg, int numvregs) {
        LOG_ERROR_IF(COMMON, !(WriteBack), "Writeback is required for VSTMDB");
        write_vreg_store_op(0x80 | 0x040 | 0x10, ptr, firstvreg >= D0, WriteBack, firstvreg, numvregs);
    }

    void armx_emitter::VLDR(arm_reg Dest, arm_reg Base, std::int16_t offset) {
        assert((Dest >= S0 && Dest <= D31) && "Passed Invalid dest register to VLDR");
        assert((Base <= R15) && "Passed invalid Base register to VLDR");

        bool Add = offset >= 0 ? true : false;
        std::uint32_t imm = abs(offset);

        assert(((imm & 0xC03) == 0) && "VLDR: Offset needs to be word aligned and small enough");

        if (imm & 0xC03)
            LOG_ERROR(COMMON, "VLDR: Bad offset %08x", imm);

        bool single_reg = Dest < D0;

        Dest = subbase(Dest);

        if (single_reg) {
            write32(condition | (0xD << 24) | (Add << 23) | ((Dest & 0x1) << 22) | (1 << 20) | (Base << 16)
                | ((Dest & 0x1E) << 11) | (10 << 8) | (imm >> 2));
        } else {
            write32(condition | (0xD << 24) | (Add << 23) | ((Dest & 0x10) << 18) | (1 << 20) | (Base << 16)
                | ((Dest & 0xF) << 12) | (11 << 8) | (imm >> 2));
        }
    }
    void armx_emitter::VSTR(arm_reg Src, arm_reg Base, std::int16_t offset) {
        assert((Src >= S0 && Src <= D31) && "Passed invalid src register to VSTR");
        assert((Base <= R15) && "Passed invalid base register to VSTR");

        bool Add = offset >= 0 ? true : false;
        std::uint32_t imm = abs(offset);

        assert(((imm & 0xC03) == 0) && "VSTR: Offset needs to be word aligned and small enough");

        if (imm & 0xC03)
            LOG_ERROR(COMMON, "VSTR: Bad offset %08x", imm);

        bool single_reg = Src < D0;

        Src = subbase(Src);

        if (single_reg) {
            write32(condition | (0xD << 24) | (Add << 23) | ((Src & 0x1) << 22) | (Base << 16)
                | ((Src & 0x1E) << 11) | (10 << 8) | (imm >> 2));
        } else {
            write32(condition | (0xD << 24) | (Add << 23) | ((Src & 0x10) << 18) | (Base << 16)
                | ((Src & 0xF) << 12) | (11 << 8) | (imm >> 2));
        }
    }

    void armx_emitter::VMRS_APSR() {
        write32(condition | 0x0EF10A10 | (15 << 12));
    }
    void armx_emitter::VMRS(arm_reg Rt) {
        write32(condition | (0xEF << 20) | (1 << 16) | (Rt << 12) | 0xA10);
    }
    void armx_emitter::VMSR(arm_reg Rt) {
        write32(condition | (0xEE << 20) | (1 << 16) | (Rt << 12) | 0xA10);
    }

    void armx_emitter::VMOV(arm_reg Dest, operand2 op2) {
        assert((context_info.bVFPv3) && "VMOV #imm requires VFPv3");
        int sz = Dest >= D0 ? (1 << 8) : 0;
        write32(condition | (0xEB << 20) | encode_vd(Dest) | (5 << 9) | sz | op2.Imm8VFP());
    }

    void armx_emitter::VMOV_neon(std::uint32_t Size, arm_reg Vd, std::uint32_t imm) {
        assert((context_info.bNEON) && "VMOV_neon #imm requires NEON");
        assert((Vd >= D0) && "VMOV_neon #imm must target a double or quad");
        bool register_quad = Vd >= Q0;

        int cmode = 0;
        int op = 0;
        operand2 op2 = IMM(0);

        std::uint32_t imm8 = imm & 0xFF;
        imm8 = imm8 | (imm8 << 8) | (imm8 << 16) | (imm8 << 24);

        if (Size == I_8) {
            imm = imm8;
        } else if (Size == I_16) {
            imm &= 0xFFFF;
            imm = imm | (imm << 16);
        }

        if ((imm & 0x000000FF) == imm) {
            op = 0;
            cmode = 0 << 1;
            op2 = IMM(imm);
        } else if ((imm & 0x0000FF00) == imm) {
            op = 0;
            cmode = 1 << 1;
            op2 = IMM(imm >> 8);
        } else if ((imm & 0x00FF0000) == imm) {
            op = 0;
            cmode = 2 << 1;
            op2 = IMM(imm >> 16);
        } else if ((imm & 0xFF000000) == imm) {
            op = 0;
            cmode = 3 << 1;
            op2 = IMM(imm >> 24);
        } else if ((imm & 0x00FF00FF) == imm && (imm >> 16) == (imm & 0x00FF)) {
            op = 0;
            cmode = 4 << 1;
            op2 = IMM(imm & 0xFF);
        } else if ((imm & 0xFF00FF00) == imm && (imm >> 16) == (imm & 0xFF00)) {
            op = 0;
            cmode = 5 << 1;
            op2 = IMM(imm & 0xFF);
        } else if ((imm & 0x0000FFFF) == (imm | 0x000000FF)) {
            op = 0;
            cmode = (6 << 1) | 0;
            op2 = IMM(imm >> 8);
        } else if ((imm & 0x00FFFFFF) == (imm | 0x0000FFFF)) {
            op = 0;
            cmode = (6 << 1) | 1;
            op2 = IMM(imm >> 16);
        } else if (imm == imm8) {
            op = 0;
            cmode = (7 << 1) | 0;
            op2 = IMM(imm & 0xFF);
        } else if (TryMakeFloatIMM8(imm, op2)) {
            op = 0;
            cmode = (7 << 1) | 1;
        } else {
            // 64-bit constant form - technically we could take a u64.
            bool canEncode = true;
            std::uint8_t imm8 = 0;
            for (int i = 0, i8 = 0; i < 32; i += 8, ++i8) {
                std::uint8_t b = (imm >> i) & 0xFF;
                if (b == 0xFF) {
                    imm8 |= 1 << i8;
                } else if (b != 0x00) {
                    canEncode = false;
                }
            }
            if (canEncode) {
                // We don't want zeros in the second lane.
                op = 1;
                cmode = 7 << 1;
                op2 = IMM(imm8 | (imm8 << 4));
            } else {
                assert((false) && "VMOV_neon #imm invalid constant value");
            }
        }

        // No condition allowed.
        write32((15 << 28) | (0x28 << 20) | encode_vd(Vd) | (cmode << 8) | (register_quad << 6) | (op << 5) | (1 << 4) | op2.Imm8ASIMD());
    }

    void armx_emitter::VMOV_neon(std::uint32_t Size, arm_reg Vd, arm_reg Rt, int lane) {
        assert((context_info.bNEON) && "VMOV_neon requires NEON");

        int opc1 = 0;
        int opc2 = 0;

        switch (Size & ~(I_SIGNED | I_UNSIGNED)) {
        case I_8:
            opc1 = 2 | (lane >> 2);
            opc2 = lane & 3;
            break;
        case I_16:
            opc1 = lane >> 1;
            opc2 = 1 | ((lane & 1) << 1);
            break;
        case I_32:
        case F_32:
            opc1 = lane & 1;
            break;
        default:
            assert((false) && "VMOV_neon unsupported size");
        }

        if (Vd < S0 && Rt >= D0 && Rt < Q0) {
            // Oh, reading to reg, our params are backwards.
            arm_reg Src = Rt;
            arm_reg Dest = Vd;

            LOG_ERROR_IF(COMMON, (Size & (I_UNSIGNED | I_SIGNED | F_32)) == 0, "Must specify I_SIGNED or I_UNSIGNED in VMOV), unless F_32");
            int U = (Size & I_UNSIGNED) ? (1 << 23) : 0;

            write32(condition | (0xE1 << 20) | U | (opc1 << 21) | EncodeVn(Src) | (Dest << 12) | (0xB << 8) | (opc2 << 5) | (1 << 4));
        } else if (Rt < S0 && Vd >= D0 && Vd < Q0) {
            arm_reg Src = Rt;
            arm_reg Dest = Vd;
            write32(condition | (0xE0 << 20) | (opc1 << 21) | EncodeVn(Dest) | (Src << 12) | (0xB << 8) | (opc2 << 5) | (1 << 4));
        } else
            assert((false) && "VMOV_neon unsupported arguments (Dx -> Rx or Rx -> Dx)");
    }

    void armx_emitter::VMOV(arm_reg Vd, arm_reg Rt, arm_reg Rt2) {
        assert((context_info.bVFP | context_info.bNEON) && "VMOV_neon requires VFP or NEON");

        if (Vd < S0 && Rt < S0 && Rt2 >= D0) {
            // Oh, reading to regs, our params are backwards.
            arm_reg Src = Rt2;
            arm_reg Dest1 = Vd;
            arm_reg Dest2 = Rt;
            write32(condition | (0xC5 << 20) | (Dest2 << 16) | (Dest1 << 12) | (0xB << 8) | EncodeVm(Src) | (1 << 4));
        } else if (Vd >= D0 && Rt < S0 && Rt2 < S0) {
            arm_reg Dest = Vd;
            arm_reg Src1 = Rt;
            arm_reg Src2 = Rt2;
            write32(condition | (0xC4 << 20) | (Src2 << 16) | (Src1 << 12) | (0xB << 8) | EncodeVm(Dest) | (1 << 4));
        } else
            assert(false && "VMOV_neon requires either Dm, Rt, Rt2 or Rt, Rt2) &&  Dm.");
    }

    void armx_emitter::VMOV(arm_reg Dest, arm_reg Src, bool high) {
        assert((Src < S0) && "This VMOV doesn't support SRC other than ARM Reg");
        assert((Dest >= D0) && "This VMOV doesn't support DEST other than VFP");

        Dest = subbase(Dest);

        write32(condition | (0xE << 24) | (high << 21) | ((Dest & 0xF) << 16) | (Src << 12)
            | (0xB << 8) | ((Dest & 0x10) << 3) | (1 << 4));
    }

    void armx_emitter::VMOV(arm_reg Dest, arm_reg Src) {
        if (Dest == Src) {
            LOG_WARN(COMMON, "VMOV %s, %s - same register", arm_reg_as_string(Src), arm_reg_as_string(Dest));
        }
        if (Dest > R15) {
            if (Src < S0) {
                if (Dest < D0) {
                    // Moving to a Neon register FROM ARM Reg
                    Dest = (arm_reg)(Dest - S0);
                    write32(condition | (0xE0 << 20) | ((Dest & 0x1E) << 15) | (Src << 12)
                        | (0xA << 8) | ((Dest & 0x1) << 7) | (1 << 4));
                    return;
                } else {
                    // Move 64bit from Arm reg
                    assert((false) && "This VMOV doesn't support moving 64bit ARM to NEON");
                    return;
                }
            }
        } else {
            if (Src > R15) {
                if (Src < D0) {
                    // Moving to ARM Reg from Neon Register
                    Src = (arm_reg)(Src - S0);
                    write32(condition | (0xE1 << 20) | ((Src & 0x1E) << 15) | (Dest << 12)
                        | (0xA << 8) | ((Src & 0x1) << 7) | (1 << 4));
                    return;
                } else {
                    // Move 64bit To Arm reg
                    assert((false) && "This VMOV doesn't support moving 64bit ARM From NEON");
                    return;
                }
            } else {
                // Move Arm reg to Arm reg
                assert((false) && "VMOV doesn't support moving ARM registers");
            }
        }
        // Moving NEON registers
        int SrcSize = Src < D0 ? 1 : Src < Q0 ? 2
                                              : 4;
        int DestSize = Dest < D0 ? 1 : Dest < Q0 ? 2
                                                 : 4;
        bool Single = DestSize == 1;
        bool Quad = DestSize == 4;

        assert((SrcSize == DestSize) && "VMOV doesn't support moving different register sizes");
        if (SrcSize != DestSize) {
            LOG_ERROR(COMMON, "SrcSize: %i (%s)  DestDize: %i (%s)", SrcSize, arm_reg_as_string(Src), DestSize, arm_reg_as_string(Dest));
        }

        Dest = subbase(Dest);
        Src = subbase(Src);

        if (Single) {
            write32(condition | (0x1D << 23) | ((Dest & 0x1) << 22) | (0x3 << 20) | ((Dest & 0x1E) << 11)
                | (0x5 << 9) | (1 << 6) | ((Src & 0x1) << 5) | ((Src & 0x1E) >> 1));
        } else {
            // Double and quad
            if (Quad) {
                assert((context_info.bNEON) && "Trying to use quad registers when you don't support ASIMD.");
                // Gets encoded as a Double register
                write32((0xF2 << 24) | ((Dest & 0x10) << 18) | (2 << 20) | ((Src & 0xF) << 16)
                    | ((Dest & 0xF) << 12) | (1 << 8) | ((Src & 0x10) << 3) | (1 << 6)
                    | ((Src & 0x10) << 1) | (1 << 4) | (Src & 0xF));

            } else {
                write32(condition | (0x1D << 23) | ((Dest & 0x10) << 18) | (0x3 << 20) | ((Dest & 0xF) << 12)
                    | (0x2D << 6) | ((Src & 0x10) << 1) | (Src & 0xF));
            }
        }
    }

    void armx_emitter::VCVT(arm_reg Dest, arm_reg Source, int flags) {
        bool single_reg = (Dest < D0) && (Source < D0);
        bool single_double = !single_reg && (Source < D0 || Dest < D0);
        bool single_to_double = Source < D0;
        int op = 0;

        if ((flags & TO_INT) && (!single_double || (single_double && !single_to_double))) {
            op = (flags & ROUND_TO_ZERO) ? 1 : 0;
        } else {
            op = (flags & IS_SIGNED) ? 1 : 0;
        }

        int op2 = 0;
        if (flags & TO_INT) {
            op2 = (flags & IS_SIGNED) ? 1 : 0;
        }

        Dest = subbase(Dest);
        Source = subbase(Source);

        if (single_double) {
            // S32<->F64
            if (flags & TO_INT) {
                if (single_to_double) {
                    write32(condition | (0x1D << 23) | ((Dest & 0x10) << 18) | (0x7 << 19)
                        | ((Dest & 0xF) << 12) | (op << 7) | (0x2D << 6) | ((Source & 0x1) << 5) | (Source >> 1));
                } else {
                    write32(condition | (0x1D << 23) | ((Dest & 0x1) << 22) | (0x7 << 19) | ((flags & TO_INT) << 18) | (op2 << 16)
                        | ((Dest & 0x1E) << 11) | (op << 7) | (0x2D << 6) | ((Source & 0x10) << 1) | (Source & 0xF));
                }
            }
            // F32<->F64
            else {
                if (single_to_double) {
                    write32(condition | (0x1D << 23) | ((Dest & 0x10) << 18) | (0x3 << 20) | (0x7 << 16)
                            | ((Dest & 0xF) << 12) | (0x2B << 6) | ((Source & 0x1) << 5) | (Source >> 1));
                } else {
                    write32(condition | (0x1D << 23) | ((Dest & 0x1) << 22) | (0x3 << 20) | (0x7 << 16)
                            | ((Dest & 0x1E) << 11) | (0x2F << 6) | ((Source & 0x10) << 1) | (Source & 0xF));
                }
            }
        } else if (single_reg) {
            write32(condition | (0x1D << 23) | ((Dest & 0x1) << 22) | (0x7 << 19) | ((flags & TO_INT) << 18) | (op2 << 16)
                | ((Dest & 0x1E) << 11) | (op << 7) | (0x29 << 6) | ((Source & 0x1) << 5) | (Source >> 1));
        } else {
            write32(condition | (0x1D << 23) | ((Dest & 0x10) << 18) | (0x7 << 19) | ((flags & TO_INT) << 18) | (op2 << 16)
                | ((Dest & 0xF) << 12) | (1 << 8) | (op << 7) | (0x29 << 6) | ((Source & 0x10) << 1) | (Source & 0xF));
        }
    }

    void armx_emitter::VABA(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");
        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | ((Size & I_UNSIGNED ? 1 : 0) << 24) | EncodeVn(Vn)
            | (encodedSize(Size) << 20) | encode_vd(Vd) | (0x71 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }

    void armx_emitter::VABAL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vn >= D0 && Vn < Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vm >= D0 && Vm < Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");

        write32((0xF2 << 24) | ((Size & I_UNSIGNED ? 1 : 0) << 24) | (1 << 23) | EncodeVn(Vn)
            | (encodedSize(Size) << 20) | encode_vd(Vd) | (0x50 << 4) | EncodeVm(Vm));
    }

    void armx_emitter::VABD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        bool register_quad = Vd >= Q0;

        if (Size & F_32)
            write32((0xF3 << 24) | (1 << 21) | EncodeVn(Vn) | encode_vd(Vd) | (0xD << 8) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | ((Size & I_UNSIGNED ? 1 : 0) << 24) | EncodeVn(Vn)
                | (encodedSize(Size) << 20) | encode_vd(Vd) | (0x70 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }

    void armx_emitter::VABDL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vn >= D0 && Vn < Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vm >= D0 && Vm < Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");

        write32((0xF2 << 24) | ((Size & I_UNSIGNED ? 1 : 0) << 24) | (1 << 23) | EncodeVn(Vn)
            | (encodedSize(Size) << 20) | encode_vd(Vd) | (0x70 << 4) | EncodeVm(Vm));
    }

    void armx_emitter::VABS(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB1 << 16) | (encodedSize(Size) << 18) | encode_vd(Vd)
            | ((Size & F_32 ? 1 : 0) << 10) | (0x30 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }

    void armx_emitter::VACGE(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        // Only Float
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | EncodeVn(Vn) | encode_vd(Vd)
            | (0xD1 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }

    void armx_emitter::VACGT(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        // Only Float
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (1 << 21) | EncodeVn(Vn) | encode_vd(Vd)
            | (0xD1 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }

    void armx_emitter::VACLE(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        VACGE(Vd, Vm, Vn);
    }

    void armx_emitter::VACLT(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        VACGT(Vd, Vn, Vm);
    }

    void armx_emitter::VADD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        if (Size & F_32)
            write32((0xF2 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0xD << 8) | (register_quad << 6) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd)
                | (0x8 << 8) | (register_quad << 6) | EncodeVm(Vm));
    }

    void armx_emitter::VADDHN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd < Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vn >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vm >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");

        write32((0xF2 << 24) | (1 << 23) | (encodedSize(Size) << 20) | EncodeVn(Vn)
            | encode_vd(Vd) | (0x80 << 4) | EncodeVm(Vm));
    }

    void armx_emitter::VADDL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vn >= D0 && Vn < Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vm >= D0 && Vm < Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");

        write32((0xF2 << 24) | ((Size & I_UNSIGNED ? 1 : 0) << 24) | (1 << 23) | (encodedSize(Size) << 20) | EncodeVn(Vn)
            | encode_vd(Vd) | EncodeVm(Vm));
    }
    void armx_emitter::VADDW(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vn >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vm >= D0 && Vm < Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");

        write32((0xF2 << 24) | ((Size & I_UNSIGNED ? 1 : 0) << 24) | (1 << 23) | (encodedSize(Size) << 20) | EncodeVn(Vn)
            | encode_vd(Vd) | (1 << 8) | EncodeVm(Vm));
    }
    void armx_emitter::VAND(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Vd == Vn && Vn == Vm)), "All operands the same for %s is a nop");
        // LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");
        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0x11 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VBIC(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        //  LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");
        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (1 << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x11 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VEOR(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, Vd < D0, "Pass invalid register to %i", static_cast<int>(Vd));
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0x11 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VBIF(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        // LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");
        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (3 << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x11 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VBIT(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        // LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");
        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (2 << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x11 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VBSL(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        // LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");
        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (1 << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x11 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCEQ(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        if (Size & F_32)
            write32((0xF2 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0xE0 << 4) | (register_quad << 6) | EncodeVm(Vm));
        else
            write32((0xF3 << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd)
                | (0x81 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCEQ(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (0xB << 20) | (encodedSize(Size) << 18) | (1 << 16)
            | encode_vd(Vd) | ((Size & F_32 ? 1 : 0) << 10) | (0x10 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCGE(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        if (Size & F_32)
            write32((0xF3 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0xE0 << 4) | (register_quad << 6) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | ((Size & I_UNSIGNED ? 1 : 0) << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd)
                | (0x31 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCGE(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        write32((0xF3 << 24) | (0xB << 20) | (encodedSize(Size) << 18) | (1 << 16)
            | encode_vd(Vd) | ((Size & F_32 ? 1 : 0) << 10) | (0x8 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCGT(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        if (Size & F_32)
            write32((0xF3 << 24) | (1 << 21) | EncodeVn(Vn) | encode_vd(Vd) | (0xE0 << 4) | (register_quad << 6) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | ((Size & I_UNSIGNED ? 1 : 0) << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd)
                | (0x30 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCGT(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        write32((0xF3 << 24) | (0xD << 20) | (encodedSize(Size) << 18) | (1 << 16)
            | encode_vd(Vd) | ((Size & F_32 ? 1 : 0) << 10) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCLE(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        VCGE(Size, Vd, Vm, Vn);
    }
    void armx_emitter::VCLE(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        write32((0xF3 << 24) | (0xD << 20) | (encodedSize(Size) << 18) | (1 << 16)
            | encode_vd(Vd) | ((Size & F_32 ? 1 : 0) << 10) | (3 << 7) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCLS(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");

        bool register_quad = Vd >= Q0;
        write32((0xF3 << 24) | (0xD << 20) | (encodedSize(Size) << 18)
            | encode_vd(Vd) | (1 << 10) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCLT(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        VCGT(Size, Vd, Vm, Vn);
    }
    void armx_emitter::VCLT(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        write32((0xF3 << 24) | (0xD << 20) | (encodedSize(Size) << 18) | (1 << 16)
            | encode_vd(Vd) | ((Size & F_32 ? 1 : 0) << 10) | (0x20 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCLZ(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        write32((0xF3 << 24) | (0xD << 20) | (encodedSize(Size) << 18)
            | encode_vd(Vd) | (0x48 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VCNT(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(Size & I_8), "Can only use I_8 with %s");

        bool register_quad = Vd >= Q0;
        write32((0xF3 << 24) | (0xD << 20) | (encodedSize(Size) << 18)
            | encode_vd(Vd) | (0x90 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VDUP(std::uint32_t Size, arm_reg Vd, arm_reg Vm, std::uint8_t index) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vm >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        std::uint32_t imm4 = 0;
        if (Size & I_8)
            imm4 = (index << 1) | 1;
        else if (Size & I_16)
            imm4 = (index << 2) | 2;
        else if (Size & (I_32 | F_32))
            imm4 = (index << 3) | 4;
        write32((0xF3 << 24) | (0xB << 20) | (imm4 << 16)
            | encode_vd(Vd) | (0xC << 8) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VDUP(std::uint32_t Size, arm_reg Vd, arm_reg Rt) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Rt < S0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        Vd = subbase(Vd);
        std::uint8_t sizeEncoded = 0;
        if (Size & I_8)
            sizeEncoded = 2;
        else if (Size & I_16)
            sizeEncoded = 1;
        else if (Size & I_32)
            sizeEncoded = 0;

        write32((0xEE << 24) | (0x8 << 20) | ((sizeEncoded & 2) << 21) | (register_quad << 21)
            | ((Vd & 0xF) << 16) | (Rt << 12) | (0xB1 << 4) | ((Vd & 0x10) << 3) | ((sizeEncoded & 1) << 5));
    }
    void armx_emitter::VEXT(arm_reg Vd, arm_reg Vn, arm_reg Vm, std::uint8_t index) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (0xB << 20) | EncodeVn(Vn) | encode_vd(Vd) | (index & 0xF)
            | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VFMA(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Size == F_32), "Passed invalid size to FP-only NEON instruction");
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(context_info.bVFPv4), "Can't use %s when CPU doesn't support it");
        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0xC1 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VFMS(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Size == F_32), "Passed invalid size to FP-only NEON instruction");
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(context_info.bVFPv4), "Can't use %s when CPU doesn't support it");
        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (1 << 21) | EncodeVn(Vn) | encode_vd(Vd) | (0xC1 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VHADD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (((Size & I_UNSIGNED) ? 1 : 0) << 23) | (encodedSize(Size) << 20)
            | EncodeVn(Vn) | encode_vd(Vd) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VHSUB(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (((Size & I_UNSIGNED) ? 1 : 0) << 23) | (encodedSize(Size) << 20)
            | EncodeVn(Vn) | encode_vd(Vd) | (1 << 9) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VMAX(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        if (Size & F_32)
            write32((0xF2 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0xF0 << 4) | (register_quad << 6) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | (((Size & I_UNSIGNED) ? 1 : 0) << 23) | (encodedSize(Size) << 20)
                | EncodeVn(Vn) | encode_vd(Vd) | (0x60 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VMIN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        if (Size & F_32)
            write32((0xF2 << 24) | (2 << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0xF0 << 4) | (register_quad << 6) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | (((Size & I_UNSIGNED) ? 1 : 0) << 23) | (encodedSize(Size) << 20)
                | EncodeVn(Vn) | encode_vd(Vd) | (0x61 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VMLA(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        if (Size & F_32)
            write32((0xF2 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0xD1 << 4) | (register_quad << 6) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x90 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VMLS(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        if (Size & F_32)
            write32((0xF2 << 24) | (1 << 21) | EncodeVn(Vn) | encode_vd(Vd) | (0xD1 << 4) | (register_quad << 6) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | (1 << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x90 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VMLAL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vn >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vm >= D0 && Vm < Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");

        write32((0xF2 << 24) | ((Size & I_UNSIGNED ? 1 : 0) << 24) | (encodedSize(Size) << 20)
            | EncodeVn(Vn) | encode_vd(Vd) | (0x80 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VMLSL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vn >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vm >= D0 && Vm < Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float.");

        write32((0xF2 << 24) | ((Size & I_UNSIGNED ? 1 : 0) << 24) | (encodedSize(Size) << 20)
            | EncodeVn(Vn) | encode_vd(Vd) | (0xA0 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VMUL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        if (Size & F_32)
            write32((0xF3 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0xD1 << 4) | (register_quad << 6) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | ((Size & I_POLYNOMIAL) ? (1 << 24) : 0) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x91 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VMULL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        write32((0xF2 << 24) | (1 << 23) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0xC0 << 4) | ((Size & I_POLYNOMIAL) ? 1 << 9 : 0) | EncodeVm(Vm));
    }
    void armx_emitter::VMLA_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        // No idea if the Non-Q case here works. Not really that interested.
        if (Size & F_32)
            write32((0xF2 << 24) | (register_quad << 24) | (1 << 23) | (2 << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x14 << 4) | EncodeVm(Vm));
        else
            LOG_ERROR_IF(COMMON, !(false), "VMLA_scalar only supports float atm");
        //else
        //	write32((0xF2 << 24) | (1 << 23) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x90 << 4) | (1 << 6) | EncodeVm(Vm));
        // Unsigned support missing
    }
    void armx_emitter::VMUL_scalar(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        int VmEnc = EncodeVm(Vm);
        // No idea if the Non-Q case here works. Not really that interested.
        if (Size & F_32) // Q flag
            write32((0xF2 << 24) | (register_quad << 24) | (1 << 23) | (2 << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x94 << 4) | VmEnc);
        else
            LOG_ERROR_IF(COMMON, !(false), "VMUL_scalar only supports float atm");

        // write32((0xF2 << 24) | ((Size & I_POLYNOMIAL) ? (1 << 24) : 0) | (1 << 23) | (encodedSize(Size) << 20) |
        // EncodeVn(Vn) | encode_vd(Vd) | (0x84 << 4) | (register_quad << 6) | EncodeVm(Vm));
        // Unsigned support missing
    }

    void armx_emitter::VMVN(arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF3B << 20) | encode_vd(Vd) | (0xB << 7) | (register_quad << 6) | EncodeVm(Vm));
    }

    void armx_emitter::VNEG(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB << 20) | (encodedSize(Size) << 18) | (1 << 16) | encode_vd(Vd) | ((Size & F_32) ? 1 << 10 : 0) | (0xE << 6) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VORN(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (3 << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x11 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VORR(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Vd == Vn && Vn == Vm)), "All operands the same for %s is a nop");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (2 << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x11 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VPADAL(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB << 20) | (encodedSize(Size) << 18) | encode_vd(Vd) | (0x60 << 4) | ((Size & I_UNSIGNED) ? 1 << 7 : 0) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VPADD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        if (Size & F_32)
            write32((0xF3 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0xD0 << 4) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0xB1 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VPADDL(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB << 20) | (encodedSize(Size) << 18) | encode_vd(Vd) | (0x20 << 4) | (Size & I_UNSIGNED ? 1 << 7 : 0) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VPMAX(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        if (Size & F_32)
            write32((0xF3 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0xF0 << 4) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | (Size & I_UNSIGNED ? 1 << 24 : 0) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0xA0 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VPMIN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        if (Size & F_32)
            write32((0xF3 << 24) | (1 << 21) | EncodeVn(Vn) | encode_vd(Vd) | (0xF0 << 4) | EncodeVm(Vm));
        else
            write32((0xF2 << 24) | (Size & I_UNSIGNED ? 1 << 24 : 0) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0xA1 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VQABS(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB << 20) | (encodedSize(Size) << 18) | encode_vd(Vd) | (0x70 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VQADD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x1 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VQDMLAL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        write32((0xF2 << 24) | (1 << 23) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x90 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VQDMLSL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        write32((0xF2 << 24) | (1 << 23) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0xB0 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VQDMULH(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        write32((0xF2 << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0xB0 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VQDMULL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        write32((0xF2 << 24) | (1 << 23) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0xD0 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VQNEG(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB << 20) | (encodedSize(Size) << 18) | encode_vd(Vd) | (0x78 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VQRDMULH(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        write32((0xF3 << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0xB0 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VQRSHL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (Size & I_UNSIGNED ? 1 << 24 : 0) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x51 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VQSHL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (Size & I_UNSIGNED ? 1 << 24 : 0) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x41 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VQSUB(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (Size & I_UNSIGNED ? 1 << 24 : 0) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x21 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VRADDHN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        write32((0xF3 << 24) | (1 << 23) | ((encodedSize(Size) - 1) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x40 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VRECPE(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB << 20) | (0xB << 16) | encode_vd(Vd) | (0x40 << 4) | (Size & F_32 ? 1 << 8 : 0) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VRECPS(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | EncodeVn(Vn) | encode_vd(Vd) | (0xF1 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VRHADD(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (Size & I_UNSIGNED ? 1 << 24 : 0) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x10 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VRSHL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (Size & I_UNSIGNED ? 1 << 24 : 0) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x50 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VRSQRTE(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;
        Vd = subbase(Vd);
        Vm = subbase(Vm);

        write32((0xF3 << 24) | (0xB << 20) | ((Vd & 0x10) << 18) | (0xB << 16)
            | ((Vd & 0xF) << 12) | (9 << 7) | (Size & F_32 ? (1 << 8) : 0) | (register_quad << 6)
            | ((Vm & 0x10) << 1) | (Vm & 0xF));
    }
    void armx_emitter::VRSQRTS(arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (1 << 21) | EncodeVn(Vn) | encode_vd(Vd) | (0xF1 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VRSUBHN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        write32((0xF3 << 24) | (1 << 23) | ((encodedSize(Size) - 1) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x60 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VSHL(std::uint32_t Size, arm_reg Vd, arm_reg Vm, arm_reg Vn) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (Size & I_UNSIGNED ? 1 << 24 : 0) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x40 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }

    static int EncodeSizeShift(std::uint32_t Size, int amount, bool inverse, bool halve) {
        int sz = 0;
        switch (Size & 0xF) {
        case I_8:
            sz = 8;
            break;
        case I_16:
            sz = 16;
            break;
        case I_32:
            sz = 32;
            break;
        case I_64:
            sz = 64;
            break;
        }
        if (inverse && halve) {
            LOG_ERROR_IF(COMMON, amount > sz / 2, "Amount %d too large for narrowing shift (max %d)", amount, sz / 2);
            return (sz / 2) + (sz / 2) - amount;
        } else if (inverse) {
            return sz + (sz - amount);
        } else {
            return sz + amount;
        }
    }

    void armx_emitter::encode_shift_by_imm(std::uint32_t Size, arm_reg Vd, arm_reg Vm, int shiftAmount, std::uint8_t opcode, bool register_quad, bool inverse, bool halve) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !(!(Size & F_32)), "%s doesn't support float");
        int imm7 = EncodeSizeShift(Size, shiftAmount, inverse, halve);
        int L = (imm7 >> 6) & 1;
        int U = (Size & I_UNSIGNED) ? 1 : 0;
        std::uint32_t value = (0xF2 << 24) | (U << 24) | (1 << 23) | ((imm7 & 0x3f) << 16) | encode_vd(Vd) | (opcode << 8) | (L << 7) | (register_quad << 6) | (1 << 4) | EncodeVm(Vm);
        write32(value);
    }

    void armx_emitter::VSHL(std::uint32_t Size, arm_reg Vd, arm_reg Vm, int shiftAmount) {
        encode_shift_by_imm((Size & ~I_UNSIGNED), Vd, Vm, shiftAmount, 0x5, Vd >= Q0, false, false);
    }

    void armx_emitter::VSHLL(std::uint32_t Size, arm_reg Vd, arm_reg Vm, int shiftAmount) {
        if ((std::uint32_t)shiftAmount == (8 * (Size & 0xF))) {
            // Entirely different encoding (A2) for size == shift! Bleh.
            int sz = 0;
            switch (Size & 0xF) {
            case I_8:
                sz = 0;
                break;
            case I_16:
                sz = 1;
                break;
            case I_32:
                sz = 2;
                break;
            case I_64:
                LOG_ERROR_IF(COMMON, !(false), "Cannot VSHLL 64-bit elements");
            }
            int imm6 = 0x32 | (sz << 2);
            std::uint32_t value = (0xF3 << 24) | (1 << 23) | (imm6 << 16) | encode_vd(Vd) | (0x3 << 8) | EncodeVm(Vm);
            write32(value);
        } else {
            encode_shift_by_imm((Size & ~I_UNSIGNED), Vd, Vm, shiftAmount, 0xA, false, false, false);
        }
    }

    void armx_emitter::VSHR(std::uint32_t Size, arm_reg Vd, arm_reg Vm, int shiftAmount) {
        encode_shift_by_imm(Size, Vd, Vm, shiftAmount, 0x0, Vd >= Q0, true, false);
    }

    void armx_emitter::VSHRN(std::uint32_t Size, arm_reg Vd, arm_reg Vm, int shiftAmount) {
        // Reduce Size by 1 to encode correctly.
        encode_shift_by_imm(Size, Vd, Vm, shiftAmount, 0x8, false, true, true);
    }

    void armx_emitter::VSUB(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        if (Size & F_32)
            write32((0xF2 << 24) | (1 << 21) | EncodeVn(Vn) | encode_vd(Vd) | (0xD0 << 4) | (register_quad << 6) | EncodeVm(Vm));
        else
            write32((0xF3 << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x80 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VSUBHN(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        write32((0xF2 << 24) | (1 << 23) | ((encodedSize(Size) - 1) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x60 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VSUBL(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        write32((0xF2 << 24) | (Size & I_UNSIGNED ? 1 << 24 : 0) | (1 << 23) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x20 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VSUBW(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        write32((0xF2 << 24) | (Size & I_UNSIGNED ? 1 << 24 : 0) | (1 << 23) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x30 << 4) | EncodeVm(Vm));
    }
    void armx_emitter::VSWP(arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB << 20) | (1 << 17) | encode_vd(Vd) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VTRN(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB << 20) | (encodedSize(Size) << 18) | (1 << 17) | encode_vd(Vd) | (1 << 7) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VTST(std::uint32_t Size, arm_reg Vd, arm_reg Vn, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF2 << 24) | (encodedSize(Size) << 20) | EncodeVn(Vn) | encode_vd(Vd) | (0x81 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VUZP(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB << 20) | (encodedSize(Size) << 18) | (1 << 17) | encode_vd(Vd) | (0x10 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }
    void armx_emitter::VZIP(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= D0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");

        bool register_quad = Vd >= Q0;

        write32((0xF3 << 24) | (0xB << 20) | (encodedSize(Size) << 18) | (1 << 17) | encode_vd(Vd) | (0x18 << 4) | (register_quad << 6) | EncodeVm(Vm));
    }

    void armx_emitter::VMOVL(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vd >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vm >= D0 && Vm <= D31), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !((Size & (I_UNSIGNED | I_SIGNED)) != 0), "Must specify I_SIGNED or I_UNSIGNED in VMOVL");

        bool unsign = (Size & I_UNSIGNED) != 0;
        int imm3 = 0;
        if (Size & I_8)
            imm3 = 1;
        if (Size & I_16)
            imm3 = 2;
        if (Size & I_32)
            imm3 = 4;

        write32((0xF2 << 24) | (unsign << 24) | (1 << 23) | (imm3 << 19) | encode_vd(Vd) | (0xA1 << 4) | EncodeVm(Vm));
    }

    void armx_emitter::VMOVN(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vm >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vd >= D0 && Vd <= D31), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !((Size & I_8) == 0), "%s cannot narrow from I_8");

        // For consistency with assembler syntax and VMOVL - encode one size down.
        int halfSize = encodedSize(Size) - 1;

        write32((0xF3B << 20) | (halfSize << 18) | (1 << 17) | encode_vd(Vd) | (1 << 9) | EncodeVm(Vm));
    }

    void armx_emitter::VQMOVN(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vm >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vd >= D0 && Vd <= D31), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !((Size & (I_UNSIGNED | I_SIGNED)) != 0), "Must specify I_SIGNED or I_UNSIGNED in %s NEON");
        LOG_ERROR_IF(COMMON, !((Size & I_8) == 0), "%s cannot narrow from I_8");

        int halfSize = encodedSize(Size) - 1;
        int op = (1 << 7) | (Size & I_UNSIGNED ? 1 << 6 : 0);

        write32((0xF3B << 20) | (halfSize << 18) | (1 << 17) | encode_vd(Vd) | (1 << 9) | op | EncodeVm(Vm));
    }

    void armx_emitter::VQMOVUN(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !(Vm >= Q0), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(Vd >= D0 && Vd <= D31), "Pass invalid register to %s");
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        LOG_ERROR_IF(COMMON, !((Size & I_8) == 0), "%s cannot narrow from I_8");

        int halfSize = encodedSize(Size) - 1;
        int op = (1 << 6);

        write32((0xF3B << 20) | (halfSize << 18) | (1 << 17) | encode_vd(Vd) | (1 << 9) | op | EncodeVm(Vm));
    }

    void armx_emitter::VCVT(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        LOG_ERROR_IF(COMMON, !((Size & (I_UNSIGNED | I_SIGNED)) != 0), "Must specify I_SIGNED or I_UNSIGNED in VCVT NEON");

        bool register_quad = Vd >= Q0;
        bool toInteger = (Size & I_32) != 0;
        bool isUnsigned = (Size & I_UNSIGNED) != 0;
        int op = (toInteger << 1) | (int)isUnsigned;

        write32((0xF3 << 24) | (0xBB << 16) | encode_vd(Vd) | (0x3 << 9) | (op << 7) | (register_quad << 6) | EncodeVm(Vm));
    }

    static int RegCountToType(int nRegs, NEONAlignment align) {
        switch (nRegs) {
        case 1:
            LOG_ERROR_IF(COMMON, !(!((int)align & 1)), "align & 1 must be == 0");
            return 7;
        case 2:
            LOG_ERROR_IF(COMMON, !(!((int)align == 3)), "align must be != 3");
            return 10;
        case 3:
            LOG_ERROR_IF(COMMON, !(!((int)align & 1)), "align & 1 must be == 0");
            return 6;
        case 4:
            return 2;
        default:
            LOG_ERROR_IF(COMMON, !(false), "Invalid number of registers passed to vector load/store");
            return 0;
        }
    }

    void armx_emitter::write_vldst1(bool load, std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align, arm_reg Rm) {
        std::uint32_t spacing = RegCountToType(regCount, align); // Only support loading to 1 reg
        // Gets encoded as a double register
        Vd = subbase(Vd);

        write32((0xF4 << 24) | ((Vd & 0x10) << 18) | (load << 21) | (Rn << 16)
            | ((Vd & 0xF) << 12) | (spacing << 8) | (encodedSize(Size) << 6)
            | (align << 4) | Rm);
    }

    void armx_emitter::VLD1(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align, arm_reg Rm) {
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        write_vldst1(true, Size, Vd, Rn, regCount, align, Rm);
    }

    void armx_emitter::VST1(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align, arm_reg Rm) {
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        write_vldst1(false, Size, Vd, Rn, regCount, align, Rm);
    }

    void armx_emitter::write_vldst1_lane(bool load, std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, bool aligned, arm_reg Rm) {
        bool register_quad = Vd >= Q0;

        Vd = subbase(Vd);
        // Support quad lanes by converting to D lanes
        if (register_quad && lane > 1) {
            Vd = (arm_reg)((int)Vd + 1);
            lane -= 2;
        }
        int encSize = encodedSize(Size);
        int index_align = 0;
        switch (encSize) {
        case 0:
            index_align = lane << 1;
            break;
        case 1:
            index_align = lane << 2;
            if (aligned)
                index_align |= 1;
            break;
        case 2:
            index_align = lane << 3;
            if (aligned)
                index_align |= 3;
            break;
        default:
            break;
        }

        write32((0xF4 << 24) | (1 << 23) | ((Vd & 0x10) << 18) | (load << 21) | (Rn << 16)
            | ((Vd & 0xF) << 12) | (encSize << 10)
            | (index_align << 4) | Rm);
    }

    void armx_emitter::VLD1_lane(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, bool aligned, arm_reg Rm) {
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        write_vldst1_lane(true, Size, Vd, Rn, lane, aligned, Rm);
    }

    void armx_emitter::VST1_lane(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int lane, bool aligned, arm_reg Rm) {
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        write_vldst1_lane(false, Size, Vd, Rn, lane, aligned, Rm);
    }

    void armx_emitter::VLD1_all_lanes(std::uint32_t Size, arm_reg Vd, arm_reg Rn, bool aligned, arm_reg Rm) {
        bool register_quad = Vd >= Q0;

        Vd = subbase(Vd);

        int T = register_quad; // two D registers

        write32((0xF4 << 24) | (1 << 23) | ((Vd & 0x10) << 18) | (1 << 21) | (Rn << 16)
            | ((Vd & 0xF) << 12) | (0xC << 8) | (encodedSize(Size) << 6)
            | (T << 5) | (aligned << 4) | Rm);
    }

    /*
    void armx_emitter::VLD2(std::uint32_t Size, arm_reg Vd, arm_reg Rn, int regCount, NEONAlignment align, arm_reg Rm)
    {
        std::uint32_t spacing = 0x8; // Single spaced registers
        // Gets encoded as a double register
        Vd = subbase(Vd);

        write32((0xF4 << 24) | ((Vd & 0x10) << 18) | (1 << 21) | (Rn << 16)
                | ((Vd & 0xF) << 12) | (spacing << 8) | (encodedSize(Size) << 6)
                | (align << 4) | Rm);
    }
    */

    void armx_emitter::write_vimm(arm_reg Vd, int cmode, std::uint8_t imm, int op) {
        bool register_quad = Vd >= Q0;

        write32((0xF28 << 20) | ((imm >> 7) << 24) | (((imm >> 4) & 0x7) << 16) | (imm & 0xF) | encode_vd(Vd) | (register_quad << 6) | (op << 5) | (1 << 4) | ((cmode & 0xF) << 8));
    }

    void armx_emitter::VMOV_imm(std::uint32_t Size, arm_reg Vd, VIMMMode type, int imm) {
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        // Only let through the modes that apply.
        switch (type) {
        case VIMM___x___x:
        case VIMM__x___x_:
        case VIMM_x___x__:
        case VIMMx___x___:
            if (Size != I_32)
                goto error;
            write_vimm(Vd, (int)type, imm, 0);
            break;
        case VIMM_x_x_x_x:
        case VIMMx_x_x_x_:
            if (Size != I_16)
                goto error;
            write_vimm(Vd, (int)type, imm, 0);
            break;
        case VIMMxxxxxxxx: // replicate the byte
            if (Size != I_8)
                goto error;
            write_vimm(Vd, (int)type, imm, 0);
            break;
        case VIMMbits2bytes:
            if (Size != I_64)
                goto error;
            write_vimm(Vd, (int)type, imm, 1);
            break;
        default:
            goto error;
        }
        return;

    error:
        LOG_ERROR(COMMON, "Bad Size or type specified: Size %i Type %i", (int)Size, (int)type);
    }

    void armx_emitter::VMOV_immf(arm_reg Vd, float value) { // This only works with a select few values. I've hardcoded 1.0f.
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        std::uint8_t bits = 0;

        if (value == 0.0f) {
            VEOR(Vd, Vd, Vd);
            return;
        }

        // TODO: Do something more sophisticated here.
        if (value == 1.5f) {
            bits = 0x78;
        } else if (value == 1.0f) {
            bits = 0x70;
        } else if (value == -1.0f) {
            bits = 0xF0;
        } else {
            LOG_ERROR_IF(COMMON, !(false), "%s: Invalid floating point immediate");
        }
        write_vimm(Vd, VIMMf000f000, bits, 0);
    }

    void armx_emitter::VORR_imm(std::uint32_t Size, arm_reg Vd, VIMMMode type, int imm) {
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        // Only let through the modes that apply.
        switch (type) {
        case VIMM___x___x:
        case VIMM__x___x_:
        case VIMM_x___x__:
        case VIMMx___x___:
            if (Size != I_32)
                goto error;
            write_vimm(Vd, (int)type | 1, imm, 0);
            break;
        case VIMM_x_x_x_x:
        case VIMMx_x_x_x_:
            if (Size != I_16)
                goto error;
            write_vimm(Vd, (int)type | 1, imm, 0);
            break;
        default:
            goto error;
        }
        return;
    error:
        LOG_ERROR_IF(COMMON, !(false), "Bad Size or type specified in VORR_imm");
    }

    void armx_emitter::VBIC_imm(std::uint32_t Size, arm_reg Vd, VIMMMode type, int imm) {
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        // Only let through the modes that apply.
        switch (type) {
        case VIMM___x___x:
        case VIMM__x___x_:
        case VIMM_x___x__:
        case VIMMx___x___:
            if (Size != I_32)
                goto error;
            write_vimm(Vd, (int)type | 1, imm, 1);
            break;
        case VIMM_x_x_x_x:
        case VIMMx_x_x_x_:
            if (Size != I_16)
                goto error;
            write_vimm(Vd, (int)type | 1, imm, 1);
            break;
        default:
            goto error;
        }
        return;
    error:
        LOG_ERROR_IF(COMMON, !(false), "Bad Size or type specified in VBIC_imm");
    }

    void armx_emitter::VMVN_imm(std::uint32_t Size, arm_reg Vd, VIMMMode type, int imm) {
        LOG_ERROR_IF(COMMON, !(context_info.bNEON), "Can't use %s when CPU doesn't support it");
        // Only let through the modes that apply.
        switch (type) {
        case VIMM___x___x:
        case VIMM__x___x_:
        case VIMM_x___x__:
        case VIMMx___x___:
            if (Size != I_32)
                goto error;
            write_vimm(Vd, (int)type, imm, 1);
            break;
        case VIMM_x_x_x_x:
        case VIMMx_x_x_x_:
            if (Size != I_16)
                goto error;
            write_vimm(Vd, (int)type, imm, 1);
            break;
        default:
            goto error;
        }
        return;
    error:
        LOG_ERROR_IF(COMMON, !(false), "Bad Size or type specified in VMVN_imm");
    }

    void armx_emitter::VREVX(std::uint32_t size, std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        bool register_quad = Vd >= Q0;
        Vd = subbase(Vd);
        Vm = subbase(Vm);

        write32((0xF3 << 24) | (1 << 23) | ((Vd & 0x10) << 18) | (0x3 << 20)
            | (encodedSize(Size) << 18) | ((Vd & 0xF) << 12) | (size << 7)
            | (register_quad << 6) | ((Vm & 0x10) << 1) | (Vm & 0xF));
    }

    void armx_emitter::VREV64(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        VREVX(0, Size, Vd, Vm);
    }

    void armx_emitter::VREV32(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        VREVX(1, Size, Vd, Vm);
    }

    void armx_emitter::VREV16(std::uint32_t Size, arm_reg Vd, arm_reg Vm) {
        VREVX(2, Size, Vd, Vm);
    }

    // See page A8-878 in ARMv7-A Architecture Reference Manual

    // Dest is a Q register, Src is a D register.
    void armx_emitter::VCVTF32F16(arm_reg Dest, arm_reg Src) {
        assert((context_info.bVFPv4) && "Can't use half-float conversions when you don't support VFPv4");
        if (Dest < Q0 || Dest > Q15 || Src < D0 || Src > D15) {
            assert((context_info.bNEON) && "Bad inputs to VCVTF32F16");
            // Invalid!
        }

        Dest = subbase(Dest);
        Src = subbase(Src);

        int op = 1;
        write32((0xF3B6 << 16) | ((Dest & 0x10) << 18) | ((Dest & 0xF) << 12) | 0x600 | (op << 8) | ((Src & 0x10) << 1) | (Src & 0xF));
    }

    // UNTESTED
    // Dest is a D register, Src is a Q register.
    void armx_emitter::VCVTF16F32(arm_reg Dest, arm_reg Src) {
        assert((context_info.bVFPv4) && "Can't use half-float conversions when you don't support VFPv4");
        if (Dest < D0 || Dest > D15 || Src < Q0 || Src > Q15) {
            assert((context_info.bNEON) && "Bad inputs to VCVTF32F16");
            // Invalid!
        }
        Dest = subbase(Dest);
        Src = subbase(Src);
        int op = 0;
        write32((0xF3B6 << 16) | ((Dest & 0x10) << 18) | ((Dest & 0xF) << 12) | 0x600 | (op << 8) | ((Src & 0x10) << 1) | (Src & 0xF));
    }

    // Always clear code space with breakpoints, so that if someone accidentally executes
    // uninitialized, it just breaks into the debugger.
    void armx_codeblock::poision_memory(int offset) {
        // TODO: this isn't right for ARM!
        memset(region + offset, 0xCC, region_size - offset);
        reset_codeptr(offset);
    }
}