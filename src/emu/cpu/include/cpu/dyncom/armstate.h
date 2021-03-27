/*  armdefs.h -- ARMulator common definitions:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#pragma once

#include <array>
#include <unordered_map>
#include <common/types.h>

#include <cpu/dyncom/arm_regformat.h>

namespace eka2l1::arm {
    class dyncom_core;
    class exclusive_monitor;
}

#define TRANS_CACHE_SIZE (64 * 1024 * 2000)

// Signal levels
enum { LOW = 0, HIGH = 1, LOWHIGH = 1, HIGHLOW = 2 };

// Cache types
enum {
    NONCACHE = 0,
    DATACACHE = 1,
    INSTCACHE = 2,
};

// ARM privilege modes
enum PrivilegeMode {
    USER32MODE = 16,
    FIQ32MODE = 17,
    IRQ32MODE = 18,
    SVC32MODE = 19,
    ABORT32MODE = 23,
    UNDEF32MODE = 27,
    SYSTEM32MODE = 31
};

// ARM privilege mode register banks
enum {
    USERBANK = 0,
    FIQBANK = 1,
    IRQBANK = 2,
    SVCBANK = 3,
    ABORTBANK = 4,
    UNDEFBANK = 5,
    DUMMYBANK = 6,
    SYSTEMBANK = 7
};

// Hardware vector addresses
enum {
    ARMResetV = 0,
    ARMUndefinedInstrV = 4,
    ARMSWIV = 8,
    ARMPrefetchAbortV = 12,
    ARMDataAbortV = 16,
    ARMAddrExceptnV = 20,
    ARMIRQV = 24,
    ARMFIQV = 28,
    ARMErrorV = 32, // This is an offset, not an address!

    ARMul_ResetV = ARMResetV,
    ARMul_UndefinedInstrV = ARMUndefinedInstrV,
    ARMul_SWIV = ARMSWIV,
    ARMul_PrefetchAbortV = ARMPrefetchAbortV,
    ARMul_DataAbortV = ARMDataAbortV,
    ARMul_AddrExceptnV = ARMAddrExceptnV,
    ARMul_IRQV = ARMIRQV,
    ARMul_FIQV = ARMFIQV
};

// Coprocessor status values
enum {
    ARMul_FIRST = 0,
    ARMul_TRANSFER = 1,
    ARMul_BUSY = 2,
    ARMul_DATA = 3,
    ARMul_INTERRUPT = 4,
    ARMul_DONE = 0,
    ARMul_CANT = 1,
    ARMul_INC = 3
};

// Instruction condition codes
enum ConditionCode {
    EQ = 0,
    NE = 1,
    CS = 2,
    CC = 3,
    MI = 4,
    PL = 5,
    VS = 6,
    VC = 7,
    HI = 8,
    LS = 9,
    GE = 10,
    LT = 11,
    GT = 12,
    LE = 13,
    AL = 14,
    NV = 15,
};

// Flags for use with the APSR.
enum : std::uint32_t {
    NBIT = (1U << 31U),
    ZBIT = (1 << 30),
    CBIT = (1 << 29),
    VBIT = (1 << 28),
    QBIT = (1 << 27),
    JBIT = (1 << 24),
    EBIT = (1 << 9),
    ABIT = (1 << 8),
    IBIT = (1 << 7),
    FBIT = (1 << 6),
    TBIT = (1 << 5),

    // Masks for groups of bits in the APSR.
    MODEBITS = 0x1F,
    INTBITS = 0x1C0,
};

// Values for Emulate.
enum {
    STOP = 0,       // Stop
    CHANGEMODE = 1, // Change mode
    ONCE = 2,       // Execute just one iteration
    RUN = 3         // Continuous execution
};

struct ARMul_State final {
public:
    explicit ARMul_State(eka2l1::arm::dyncom_core *core, PrivilegeMode initial_mode);

    void ChangePrivilegeMode(std::uint32_t new_mode);
    void Reset();

    // Reads/writes data in big/little endian format based on the
    // state of the E (endian) bit in the APSR.
    std::uint8_t ReadMemory8(std::uint32_t address) const;
    std::uint16_t ReadMemory16(std::uint32_t address) const;
    std::uint32_t ReadMemory32(std::uint32_t address) const;
    std::uint64_t ReadMemory64(std::uint32_t address) const;
    std::uint32_t ReadCode(std::uint32_t address) const;
    void WriteMemory8(std::uint32_t address, std::uint8_t data);
    void WriteMemory16(std::uint32_t address, std::uint16_t data);
    void WriteMemory32(std::uint32_t address, std::uint32_t data);
    void WriteMemory64(std::uint32_t address, std::uint64_t data);

    void RaiseException(const int type, const std::uint32_t data);
    void RaiseSystemCall(std::uint32_t val);

    std::uint32_t ReadCP15Register(std::uint32_t crn, std::uint32_t opcode_1, std::uint32_t crm, std::uint32_t opcode_2) const;
    void WriteCP15Register(std::uint32_t value, std::uint32_t crn, std::uint32_t opcode_1, std::uint32_t crm, std::uint32_t opcode_2);

    eka2l1::arm::exclusive_monitor *exmonitor();

    // Whether or not the given CPU is in big endian mode (E bit is set)
    bool InBigEndianMode() const {
        return (Cpsr & (1 << 9)) != 0;
    }
    // Whether or not the given CPU is in a mode other than user mode.
    bool InAPrivilegedMode() const {
        return (Mode != USER32MODE);
    }
    // Whether or not the current CPU mode has a Saved Program Status Register
    bool CurrentModeHasSPSR() const {
        return Mode != SYSTEM32MODE && InAPrivilegedMode();
    }
    // Note that for the 3DS, a Thumb instruction will only ever be
    // two bytes in size. Thus we don't need to worry about ThumbEE
    // or Thumb-2 where instructions can be 4 bytes in length.
    std::uint32_t GetInstructionSize() const {
        return TFlag ? 2 : 4;
    }

    std::array<std::uint32_t, 16> Reg{}; // The current register file
    std::array<std::uint32_t, 2> Reg_usr{};
    std::array<std::uint32_t, 2> Reg_svc{};   // R13_SVC R14_SVC
    std::array<std::uint32_t, 2> Reg_abort{}; // R13_ABORT R14_ABORT
    std::array<std::uint32_t, 2> Reg_undef{}; // R13 UNDEF R14 UNDEF
    std::array<std::uint32_t, 2> Reg_irq{};   // R13_IRQ R14_IRQ
    std::array<std::uint32_t, 7> Reg_firq{};  // R8---R14 FIRQ
    std::array<std::uint32_t, 7> Spsr{};      // The exception psr's
    std::array<std::uint32_t, CP15_REGISTER_COUNT> CP15{};

    // FPSID, FPSCR, and FPEXC
    std::array<std::uint32_t, VFP_SYSTEM_REGISTER_COUNT> VFP{};

    // VFPv2 and VFPv3-D16 has 16 doubleword registers (D0-D16 or S0-S31).
    // VFPv3-D32/ASIMD may have up to 32 doubleword registers (D0-D31),
    // and only 32 singleword registers are accessible (S0-S31).
    std::array<std::uint32_t, 64> ExtReg{};

    std::uint32_t Emulate; // To start and stop emulation
    std::uint32_t Cpsr;    // The current PSR
    std::uint32_t Spsr_copy;
    std::uint32_t phys_pc;

    std::uint32_t Mode; // The current mode
    std::uint32_t Bank; // The current register bank

    std::uint32_t NFlag, ZFlag, CFlag, VFlag, IFFlags; // Dummy flags for speed
    unsigned int shifter_carry_out;

    std::uint32_t TFlag; // Thumb state

    unsigned long long NumInstrs; // The number of instructions executed
    std::uint64_t NumInstrsToExecute;

    unsigned NresetSig; // Reset the processor
    unsigned NfiqSig;
    unsigned NirqSig;

    unsigned abortSig;
    unsigned NtransSig;
    unsigned bigendSig;
    unsigned syscallSig;
    
    char trans_cache_buf[TRANS_CACHE_SIZE];
    size_t trans_cache_buf_top = 0;

    // TODO(bunnei): Move this cache to a better place - it should be per codeset (likely per
    // process for our purposes), not per ARMul_State (which tracks CPU core state).
    std::unordered_map<std::uint32_t, std::size_t> instruction_cache;

private:
    void ResetMPCoreCP15Registers();
    eka2l1::arm::dyncom_core *core;
};
