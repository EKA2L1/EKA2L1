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
#include <common/bytes.h>
#include <common/types.h>
#include <unordered_map>

#include <cpu/12l1r/tlb.h>
#include <cpu/dyncom/arm_regformat.h>

namespace eka2l1::arm {
    class dyncom_core;
    class core;
    class exclusive_monitor;
}

#define TRANS_CACHE_SIZE (64 * 1024 * 2000)

// Signal levels
enum { LOW = 0,
    HIGH = 1,
    LOWHIGH = 1,
    HIGHLOW = 2 };

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
    STOP = 0, // Stop
    CHANGEMODE = 1, // Change mode
    ONCE = 2, // Execute just one iteration
    RUN = 3 // Continuous execution
};

struct ARMul_State final {
public:
    explicit ARMul_State(eka2l1::arm::dyncom_core *core, PrivilegeMode initial_mode);

    void ChangePrivilegeMode(std::uint32_t new_mode);
    void Reset();

    // Reads/writes data in big/little endian format based on the
    // state of the E (endian) bit in the APSR.
    // The common case (the address is in the dyncom TLB) is inlined here so the
    // load/store handlers pay no call overhead; a miss falls through to the
    // out-of-line slow path (page-table walk, fault handling, big-endian,
    // logging). mem_cache_ is the same TLB as core->mem_cache(), cached so this
    // header needn't see the full dyncom_core. Read fast paths return the raw
    // little-endian value (matching the previous TLB-hit behaviour); writes swap
    // first so the stored bytes match.
    std::uint8_t ReadMemory8(std::uint32_t address) const {
        if (std::uint8_t *ptr = mem_cache_->lookup(address))
            return *ptr;
        return ReadMemory8Slow(address);
    }
    std::uint16_t ReadMemory16(std::uint32_t address) const {
        if (std::uint16_t *ptr = reinterpret_cast<std::uint16_t *>(mem_cache_->lookup(address)))
            return *ptr;
        return ReadMemory16Slow(address);
    }
    std::uint32_t ReadMemory32(std::uint32_t address) const {
        if (std::uint32_t *ptr = reinterpret_cast<std::uint32_t *>(mem_cache_->lookup(address)))
            return *ptr;
        return ReadMemory32Slow(address);
    }
    std::uint64_t ReadMemory64(std::uint32_t address) const {
        if (std::uint64_t *ptr = reinterpret_cast<std::uint64_t *>(mem_cache_->lookup(address)))
            return *ptr;
        return ReadMemory64Slow(address);
    }
    std::uint32_t ReadCode(std::uint32_t address) const;
    void WriteMemory8(std::uint32_t address, std::uint8_t data) {
        if (std::uint8_t *ptr = mem_cache_->lookup(address)) {
            *ptr = data;
            return;
        }
        WriteMemory8Slow(address, data);
    }
    void WriteMemory16(std::uint32_t address, std::uint16_t data) {
        if (InBigEndianMode())
            data = eka2l1::common::byte_swap(data);
        if (std::uint16_t *ptr = reinterpret_cast<std::uint16_t *>(mem_cache_->lookup(address))) {
            *ptr = data;
            return;
        }
        WriteMemory16Slow(address, data);
    }
    void WriteMemory32(std::uint32_t address, std::uint32_t data) {
        if (InBigEndianMode())
            data = eka2l1::common::byte_swap(data);
        if (std::uint32_t *ptr = reinterpret_cast<std::uint32_t *>(mem_cache_->lookup(address))) {
            *ptr = data;
            return;
        }
        WriteMemory32Slow(address, data);
    }
    void WriteMemory64(std::uint32_t address, std::uint64_t data) {
        if (InBigEndianMode())
            data = eka2l1::common::byte_swap(data);
        if (std::uint64_t *ptr = reinterpret_cast<std::uint64_t *>(mem_cache_->lookup(address))) {
            *ptr = data;
            return;
        }
        WriteMemory64Slow(address, data);
    }

    // Cursor for bulk word transfers (LDM/STM). A register-list transfer touches a
    // run of contiguous addresses that, in practice, all fall in one guest page, yet
    // the naive loop pays a full TLB lookup per word. The cursor caches the resolved
    // host page so a same-page run costs one lookup instead of one per word, and it
    // re-resolves automatically when the run crosses a page boundary. Semantics are
    // identical to ReadMemory32/WriteMemory32 (raw little-endian on a TLB hit, the
    // out-of-line slow path on a miss, and the same big-endian write swap), so it is
    // a drop-in replacement inside a single instruction's transfer loop.
    struct block_cursor {
        std::uint8_t *page_host = nullptr;
        std::uint32_t page_base = 1; // sentinel: never equals a page-aligned address
    };

    std::uint32_t ReadMemory32Block(std::uint32_t address, block_cursor &c) const {
        const std::uint32_t page_off = address & static_cast<std::uint32_t>(mem_cache_->page_mask);
        if (c.page_host && (address - page_off) == c.page_base)
            return *reinterpret_cast<std::uint32_t *>(c.page_host + page_off);
        if (std::uint8_t *ptr = mem_cache_->lookup(address)) {
            c.page_host = ptr - page_off;
            c.page_base = address - page_off;
            return *reinterpret_cast<std::uint32_t *>(ptr);
        }
        c.page_host = nullptr;
        return ReadMemory32Slow(address);
    }

    void WriteMemory32Block(std::uint32_t address, std::uint32_t data, block_cursor &c) {
        if (InBigEndianMode())
            data = eka2l1::common::byte_swap(data);
        const std::uint32_t page_off = address & static_cast<std::uint32_t>(mem_cache_->page_mask);
        if (c.page_host && (address - page_off) == c.page_base) {
            *reinterpret_cast<std::uint32_t *>(c.page_host + page_off) = data;
            return;
        }
        if (std::uint8_t *ptr = mem_cache_->lookup(address)) {
            c.page_host = ptr - page_off;
            c.page_base = address - page_off;
            *reinterpret_cast<std::uint32_t *>(ptr) = data;
            return;
        }
        c.page_host = nullptr;
        WriteMemory32Slow(address, data);
    }

    // Slow paths (TLB miss): out-of-line in armstate.cpp. Reads apply the
    // big-endian swap themselves; writes receive data already swapped.
    std::uint8_t ReadMemory8Slow(std::uint32_t address) const;
    std::uint16_t ReadMemory16Slow(std::uint32_t address) const;
    std::uint32_t ReadMemory32Slow(std::uint32_t address) const;
    std::uint64_t ReadMemory64Slow(std::uint32_t address) const;
    void WriteMemory8Slow(std::uint32_t address, std::uint8_t data);
    void WriteMemory16Slow(std::uint32_t address, std::uint16_t data);
    void WriteMemory32Slow(std::uint32_t address, std::uint32_t data);
    void WriteMemory64Slow(std::uint32_t address, std::uint64_t data);

    void RaiseException(const int type, const std::uint32_t data);
    void RaiseSystemCall(std::uint32_t val);

    std::uint32_t ReadCP15Register(std::uint32_t crn, std::uint32_t opcode_1, std::uint32_t crm, std::uint32_t opcode_2) const;
    void WriteCP15Register(std::uint32_t value, std::uint32_t crn, std::uint32_t opcode_1, std::uint32_t crm, std::uint32_t opcode_2);

    eka2l1::arm::exclusive_monitor *exmonitor();
    eka2l1::arm::core *parent();

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
    std::array<std::uint32_t, 2> Reg_svc{}; // R13_SVC R14_SVC
    std::array<std::uint32_t, 2> Reg_abort{}; // R13_ABORT R14_ABORT
    std::array<std::uint32_t, 2> Reg_undef{}; // R13 UNDEF R14 UNDEF
    std::array<std::uint32_t, 2> Reg_irq{}; // R13_IRQ R14_IRQ
    std::array<std::uint32_t, 7> Reg_firq{}; // R8---R14 FIRQ
    std::array<std::uint32_t, 7> Spsr{}; // The exception psr's
    std::array<std::uint32_t, CP15_REGISTER_COUNT> CP15{};

    // FPSID, FPSCR, and FPEXC
    std::array<std::uint32_t, VFP_SYSTEM_REGISTER_COUNT> VFP{};

    // VFPv2 and VFPv3-D16 has 16 doubleword registers (D0-D16 or S0-S31).
    // VFPv3-D32/ASIMD may have up to 32 doubleword registers (D0-D31),
    // and only 32 singleword registers are accessible (S0-S31).
    std::array<std::uint32_t, 64> ExtReg{};

    std::uint32_t Emulate; // To start and stop emulation
    std::uint32_t Cpsr; // The current PSR
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

    // Data TLB shared with the owning dyncom_core (== core->mem_cache()), cached
    // here so the inline memory accessors above don't need the full dyncom_core
    // definition. Set by dyncom_core right after construction.
    eka2l1::arm::r12l1::tlb *mem_cache_ = nullptr;

    char trans_cache_buf[TRANS_CACHE_SIZE];
    size_t trans_cache_buf_top = 0;

    // Translated basic blocks are tagged with the address space (asid) they were
    // decoded in, so blocks from different processes coexist and survive the
    // frequent guest thread/process switches instead of being thrown away on
    // every context switch. Key = (asid << 32) | virtual_pc. The asid is pushed
    // in by the scheduler through dyncom_core::set_asid on every process switch.
    // In the multiple memory model (used by all current frontends) asids are
    // never recycled within a session, so cached blocks stay valid until the
    // code itself changes (handled separately via imb_range / clear).
    std::unordered_map<std::uint64_t, std::size_t> instruction_cache;
    std::uint32_t instruction_cache_asid = 0;

#ifdef EKA2L1_DYNCOM_PROFILE
    // Guest-execution profiler scratch (see arm_dyncom_interpreter.cpp): the
    // previous executed opcode index within the current block and the running
    // block length. -1 prev means "block start".
    int prof_prev = -1;
    std::uint32_t prof_block_len = 0;
#endif

    std::uint64_t make_instruction_cache_key(std::uint32_t vaddr) const {
        return (static_cast<std::uint64_t>(instruction_cache_asid) << 32) | vaddr;
    }

    // Direct-mapped L1 in front of instruction_cache. DISPATCH runs on every
    // taken branch; an index+compare here turns the common case into a hit that
    // skips the unordered_map hash/bucket-walk/modulo. Entries are tagged with
    // the full (asid|vpc) key so they coexist across processes; they only need
    // clearing when the block map / translation buffer is flushed. Keys are
    // seeded to a value no real key can take (real high word = asid <= 255).
    static constexpr std::size_t BLOCK_L1_BITS = 11; // 2048 entries (~32 KB)
    static constexpr std::size_t BLOCK_L1_COUNT = 1 << BLOCK_L1_BITS;
    static constexpr std::uint64_t BLOCK_L1_EMPTY = ~static_cast<std::uint64_t>(0);

    struct block_l1_entry {
        std::uint64_t key;
        std::size_t ptr;
    };
    block_l1_entry block_l1_cache[BLOCK_L1_COUNT];

    void flush_block_l1_cache() {
        for (std::size_t i = 0; i < BLOCK_L1_COUNT; i++) {
            block_l1_cache[i].key = BLOCK_L1_EMPTY;
        }
    }

    static std::size_t block_l1_index(std::uint64_t key) {
        return (key ^ (key >> 32)) & (BLOCK_L1_COUNT - 1);
    }

private:
    void ResetMPCoreCP15Registers();
    eka2l1::arm::dyncom_core *core;
};
