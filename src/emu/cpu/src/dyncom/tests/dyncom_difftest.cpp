/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * Differential test harness for the dyncom interpreter.
 *
 * Standalone host tool (no Catch2 dependency) that runs randomized ARM
 * instruction cases through the dyncom interpreter and checks the guest-visible
 * result against:
 *   (a) an independent golden ALU model (data-processing semantics + the ARM
 *       barrel-shifter carry-out rules) -- the fixed reference that the
 *       semantic optimizations (shifter specialization, lazy flags, inline
 *       AddWithCarry) must keep matching, and
 *   (b) a second dyncom instance (self-A/B) -- so a dispatch-shape optimization
 *       gated behind a flag can be proven behaviour-preserving by toggling it
 *       on one side.
 *
 * A negative-control case deliberately perturbs the result to prove the
 * comparator actually detects a divergence (a harness that can never fail is
 * useless).
 *
 * Build:  cmake -DEKA2L1_BUILD_DYNCOM_DIFFTEST=ON ...   (needs dynarmic)
 * Run:    scripts/cpu_difftest.sh   (exits non-zero on the first divergence)
 */

#include <cpu/arm_dynarmic.h>
#include <cpu/dyncom/arm_dyncom.h>
#include <cpu/dyncom/vfp/asm_vfp.h>
#include <cpu/dyncom/vfp/vfp.h>
#include <cpu/12l1r/exclusive_monitor.h>

#include <common/types.h>

#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <vector>

using namespace eka2l1;
using namespace eka2l1::arm;

namespace {

constexpr std::uint32_t MEM_SIZE = 0x10000; // 64 KB flat memory
constexpr std::size_t PAGE_BITS = 12;
constexpr std::uint32_t PAGE_MASK = (1u << PAGE_BITS) - 1;

// ---------------------------------------------------------------------------
// dyncom core over a flat memory buffer
// ---------------------------------------------------------------------------
struct diff_env {
    std::vector<std::uint8_t> mem;
    r12l1::exclusive_monitor monitor;
    diff_env()
        : mem(MEM_SIZE, 0)
        , monitor(1) {
    }
};

std::unique_ptr<dyncom_core> make_core(diff_env &env) {
    auto core = std::make_unique<dyncom_core>(&env.monitor, PAGE_BITS);
    std::uint8_t *base = env.mem.data();
    dyncom_core *cp = core.get();

    auto seed_tlb = [cp, base](std::uint32_t addr) {
        const std::uint32_t page = addr & ~PAGE_MASK;
        cp->set_tlb_page(page, base + page, prot_read_write);
    };

    core->read_code = [base](const address a, std::uint32_t *r) -> bool {
        if (a + 4 > MEM_SIZE)
            return false;
        std::memcpy(r, base + a, 4);
        return true;
    };
    core->read_8bit = [base, seed_tlb](const address a, std::uint8_t *r) -> bool {
        if (a >= MEM_SIZE)
            return false;
        *r = base[a];
        seed_tlb(a);
        return true;
    };
    core->read_16bit = [base, seed_tlb](const address a, std::uint16_t *r) -> bool {
        if (a + 2 > MEM_SIZE)
            return false;
        std::memcpy(r, base + a, 2);
        seed_tlb(a);
        return true;
    };
    core->read_32bit = [base, seed_tlb](const address a, std::uint32_t *r) -> bool {
        if (a + 4 > MEM_SIZE)
            return false;
        std::memcpy(r, base + a, 4);
        seed_tlb(a);
        return true;
    };
    core->read_64bit = [base, seed_tlb](const address a, std::uint64_t *r) -> bool {
        if (a + 8 > MEM_SIZE)
            return false;
        std::memcpy(r, base + a, 8);
        seed_tlb(a);
        return true;
    };
    core->write_8bit = [base, seed_tlb](const address a, std::uint8_t *v) -> bool {
        if (a >= MEM_SIZE)
            return false;
        base[a] = *v;
        seed_tlb(a);
        return true;
    };
    core->write_16bit = [base, seed_tlb](const address a, std::uint16_t *v) -> bool {
        if (a + 2 > MEM_SIZE)
            return false;
        std::memcpy(base + a, v, 2);
        seed_tlb(a);
        return true;
    };
    core->write_32bit = [base, seed_tlb](const address a, std::uint32_t *v) -> bool {
        if (a + 4 > MEM_SIZE)
            return false;
        std::memcpy(base + a, v, 4);
        seed_tlb(a);
        return true;
    };
    core->write_64bit = [base, seed_tlb](const address a, std::uint64_t *v) -> bool {
        if (a + 8 > MEM_SIZE)
            return false;
        std::memcpy(base + a, v, 8);
        seed_tlb(a);
        return true;
    };

    core->exception_handler = [](exception_type, const std::uint32_t) -> bool { return false; };
    core->system_call_handler = [](const std::uint32_t) {};

    return core;
}

// ---------------------------------------------------------------------------
// Guest-visible state snapshot
// ---------------------------------------------------------------------------
struct cpu_state {
    std::uint32_t reg[16];
    std::uint32_t cpsr;

    bool operator==(const cpu_state &o) const {
        return std::memcmp(this, &o, sizeof(cpu_state)) == 0;
    }
};

cpu_state read_state(dyncom_core &c) {
    cpu_state s{};
    for (std::size_t i = 0; i < 16; i++) {
        s.reg[i] = c.get_reg(i);
    }
    s.cpsr = c.get_cpsr();
    return s;
}

void write_state(dyncom_core &c, const cpu_state &s) {
    for (std::size_t i = 0; i < 16; i++) {
        c.set_reg(i, s.reg[i]);
    }
    c.set_cpsr(s.cpsr);
}

// ---------------------------------------------------------------------------
// Golden ALU model (independent reference) -- ARM data-processing
// ---------------------------------------------------------------------------
constexpr std::uint32_t N_BIT = 1u << 31;
constexpr std::uint32_t Z_BIT = 1u << 30;
constexpr std::uint32_t C_BIT = 1u << 29;
constexpr std::uint32_t V_BIT = 1u << 28;

bool cond_passed(std::uint32_t cond, std::uint32_t cpsr) {
    const bool n = cpsr & N_BIT, z = cpsr & Z_BIT, c = cpsr & C_BIT, v = cpsr & V_BIT;
    switch (cond) {
    case 0x0: return z;                       // EQ
    case 0x1: return !z;                      // NE
    case 0x2: return c;                       // CS
    case 0x3: return !c;                      // CC
    case 0x4: return n;                       // MI
    case 0x5: return !n;                      // PL
    case 0x6: return v;                       // VS
    case 0x7: return !v;                      // VC
    case 0x8: return c && !z;                 // HI
    case 0x9: return !c || z;                 // LS
    case 0xA: return n == v;                  // GE
    case 0xB: return n != v;                  // LT
    case 0xC: return !z && (n == v);          // GT
    case 0xD: return z || (n != v);           // LE
    case 0xE: return true;                    // AL
    default: return true;
    }
}

// Independent add-with-carry (64-bit; deliberately NOT the interpreter's
// __builtin version, so it cross-checks rather than mirrors a bug).
std::uint32_t golden_addc(std::uint32_t a, std::uint32_t b, std::uint32_t cin, bool *carry, bool *overflow) {
    const std::uint64_t usum = (std::uint64_t)a + (std::uint64_t)b + (std::uint64_t)cin;
    const std::int64_t ssum = (std::int64_t)(std::int32_t)a + (std::int64_t)(std::int32_t)b + (std::int64_t)cin;
    const std::uint32_t r = (std::uint32_t)usum;
    *carry = (usum >> 32) != 0;
    *overflow = ((std::int64_t)(std::int32_t)r != ssum);
    return r;
}

// Barrel shifter (immediate-specified shifts + the immediate-operand rotate).
// Returns the operand value and the shifter carry-out.
std::uint32_t golden_shifter(std::uint32_t op2_field, bool is_immediate, const std::uint32_t *regs,
    bool carry_in, bool *carry_out) {
    if (is_immediate) {
        const std::uint32_t imm8 = op2_field & 0xFF;
        const std::uint32_t rot = ((op2_field >> 8) & 0xF) * 2;
        const std::uint32_t val = (rot == 0) ? imm8 : ((imm8 >> rot) | (imm8 << (32 - rot)));
        *carry_out = (rot == 0) ? carry_in : ((val >> 31) & 1);
        return val;
    }

    const std::uint32_t rm = regs[op2_field & 0xF];
    const std::uint32_t shift_type = (op2_field >> 5) & 0x3;

    // Register-specified shift (bit 4 set): the amount is the low byte of Rs and
    // the >=32 cases have their own carry-out rules.
    if (op2_field & 0x10) {
        const std::uint32_t amt = regs[(op2_field >> 8) & 0xF] & 0xFF;
        if (amt == 0) {
            *carry_out = carry_in;
            return rm;
        }
        switch (shift_type) {
        case 0: // LSL
            if (amt < 32) { *carry_out = (rm >> (32 - amt)) & 1; return rm << amt; }
            if (amt == 32) { *carry_out = rm & 1; return 0; }
            *carry_out = false; return 0;
        case 1: // LSR
            if (amt < 32) { *carry_out = (rm >> (amt - 1)) & 1; return rm >> amt; }
            if (amt == 32) { *carry_out = (rm >> 31) & 1; return 0; }
            *carry_out = false; return 0;
        case 2: // ASR
            if (amt < 32) { *carry_out = (rm >> (amt - 1)) & 1; return (std::uint32_t)((std::int32_t)rm >> amt); }
            *carry_out = (rm >> 31) & 1; return (rm & N_BIT) ? 0xFFFFFFFF : 0;
        default: { // ROR
            const std::uint32_t a = amt & 0x1F;
            if (a == 0) { *carry_out = (rm >> 31) & 1; return rm; } // amount a non-zero multiple of 32
            *carry_out = (rm >> (a - 1)) & 1;
            return (rm >> a) | (rm << (32 - a));
        }
        }
    }

    const std::uint32_t amount = (op2_field >> 7) & 0x1F; // immediate shift amount

    switch (shift_type) {
    case 0: // LSL
        if (amount == 0) {
            *carry_out = carry_in;
            return rm;
        }
        *carry_out = (rm >> (32 - amount)) & 1;
        return rm << amount;
    case 1: // LSR (amount 0 means 32)
        if (amount == 0) {
            *carry_out = (rm >> 31) & 1;
            return 0;
        }
        *carry_out = (rm >> (amount - 1)) & 1;
        return rm >> amount;
    case 2: // ASR (amount 0 means 32)
        if (amount == 0) {
            *carry_out = (rm >> 31) & 1;
            return (rm & N_BIT) ? 0xFFFFFFFF : 0;
        }
        *carry_out = (rm >> (amount - 1)) & 1;
        return (std::uint32_t)((std::int32_t)rm >> amount);
    default:  // ROR / RRX (amount 0 means RRX)
        if (amount == 0) { // RRX
            *carry_out = rm & 1;
            return (rm >> 1) | (carry_in ? N_BIT : 0);
        }
        *carry_out = (rm >> (amount - 1)) & 1;
        return (rm >> amount) | (rm << (32 - amount));
    }
}

// Apply one data-processing instruction to a golden state. Returns the expected
// post-state. Mirrors ARM semantics for the 16 DP opcodes (no PC writes, no
// S+Rd==15 SPSR restore -- the generator never emits those).
cpu_state golden_data_processing(std::uint32_t inst, const cpu_state &in) {
    cpu_state out = in;
    out.reg[15] = in.reg[15] + 4; // PC advances one ARM instruction

    const std::uint32_t cond = inst >> 28;
    if (!cond_passed(cond, in.cpsr)) {
        return out; // condition failed: only PC advanced
    }

    const bool is_imm = (inst >> 25) & 1;
    const std::uint32_t opcode = (inst >> 21) & 0xF;
    const bool S = (inst >> 20) & 1;
    const std::uint32_t rn = (inst >> 16) & 0xF;
    const std::uint32_t rd = (inst >> 12) & 0xF;
    const std::uint32_t op2_field = inst & 0xFFF;

    const bool cin = (in.cpsr & C_BIT) != 0;
    bool shifter_carry = cin;
    const std::uint32_t b = golden_shifter(op2_field, is_imm, in.reg, cin, &shifter_carry);
    const std::uint32_t a = in.reg[rn];

    std::uint32_t result = 0;
    bool write_rd = true;
    bool logical = true;
    bool carry = cin, overflow = (in.cpsr & V_BIT) != 0;

    switch (opcode) {
    case 0x0: result = a & b; break;                                   // AND
    case 0x1: result = a ^ b; break;                                   // EOR
    case 0x2: result = golden_addc(a, ~b, 1, &carry, &overflow); logical = false; break;       // SUB
    case 0x3: result = golden_addc(~a, b, 1, &carry, &overflow); logical = false; break;       // RSB
    case 0x4: result = golden_addc(a, b, 0, &carry, &overflow); logical = false; break;        // ADD
    case 0x5: result = golden_addc(a, b, cin, &carry, &overflow); logical = false; break;      // ADC
    case 0x6: result = golden_addc(a, ~b, cin, &carry, &overflow); logical = false; break;     // SBC
    case 0x7: result = golden_addc(~a, b, cin, &carry, &overflow); logical = false; break;     // RSC
    case 0x8: result = a & b; write_rd = false; break;                 // TST
    case 0x9: result = a ^ b; write_rd = false; break;                 // TEQ
    case 0xA: result = golden_addc(a, ~b, 1, &carry, &overflow); logical = false; write_rd = false; break; // CMP
    case 0xB: result = golden_addc(a, b, 0, &carry, &overflow); logical = false; write_rd = false; break;  // CMN
    case 0xC: result = a | b; break;                                   // ORR
    case 0xD: result = b; break;                                       // MOV
    case 0xE: result = a & ~b; break;                                  // BIC
    case 0xF: result = ~b; break;                                      // MVN
    default: break;
    }

    if (write_rd) {
        out.reg[rd] = result;
    }

    if (S) {
        std::uint32_t cpsr = out.cpsr & ~(N_BIT | Z_BIT | C_BIT | V_BIT);
        if (result & N_BIT) cpsr |= N_BIT;
        if (result == 0) cpsr |= Z_BIT;
        if (logical) {
            if (shifter_carry) cpsr |= C_BIT;
            cpsr |= (in.cpsr & V_BIT); // V unchanged for logical
        } else {
            if (carry) cpsr |= C_BIT;
            if (overflow) cpsr |= V_BIT;
        }
        out.cpsr = cpsr;
    }

    return out;
}

// ---------------------------------------------------------------------------
// Random instruction + state generation
// ---------------------------------------------------------------------------
struct rng {
    std::mt19937 e;
    explicit rng(std::uint32_t seed)
        : e(seed) {
    }
    std::uint32_t u32() { return e(); }
    std::uint32_t range(std::uint32_t n) { return e() % n; }
    bool flip() { return e() & 1; }
};

std::uint32_t random_normal_f32(rng &r) {
    // Keep most products away from overflow/underflow so the host envelope is
    // exercised heavily, while still varying signs and every significand bit.
    const std::uint32_t sign = r.u32() & 0x80000000u;
    const std::uint32_t exponent = 80u + r.range(96u);
    return sign | (exponent << 23) | (r.u32() & 0x007FFFFFu);
}

std::uint32_t run_vfp_mac_differential(diff_env &env_a, diff_env &env_b,
    dyncom_core &core_a, dyncom_core &core_b, std::uint32_t base_seed,
    std::uint32_t count) {
    struct mac_op {
        const char *name;
        std::uint32_t inst;
    };
    // vmla/vmls/vnmla/vnmls.f32 s0, s1, s2 (ARM state, AL condition).
    static constexpr mac_op ops[] = {
        { "vmla.f32", 0xEE000A81u },
        { "vmls.f32", 0xEE000AC1u },
        { "vnmla.f32", 0xEE100AC1u },
        { "vnmls.f32", 0xEE100A81u },
    };

    const std::uint32_t cases = count < 50000u ? count : 50000u;
    std::uint32_t failures = 0;
    vfp_reset_single_host_fast_hits_for_test();

    for (std::uint32_t i = 0; i < cases && failures < 20; ++i) {
        const std::uint32_t case_seed = base_seed ^ (0x9E3779B9u * (i + 1u));
        rng r(case_seed);
        const std::uint32_t accumulator = random_normal_f32(r);
        const std::uint32_t multiplicand = random_normal_f32(r);
        const std::uint32_t multiplier = random_normal_f32(r);

        for (const mac_op &op : ops) {
            std::memcpy(env_a.mem.data(), &op.inst, 4);
            std::memcpy(env_b.mem.data(), &op.inst, 4);
            core_a.imb_range(0, 8);
            core_b.imb_range(0, 8);

            for (dyncom_core *core : { &core_a, &core_b }) {
                core->set_cpsr(0x10); // USER mode, ARM state
                core->set_pc(0);
                core->set_fpscr(0);   // RN, gradual underflow, propagated NaNs
                core->set_vfp(0, accumulator);
                core->set_vfp(1, multiplicand);
                core->set_vfp(2, multiplier);
            }

            vfp_set_single_host_fast_for_test(false);
            core_a.run(1);
            vfp_set_single_host_fast_for_test(true);
            core_b.run(1);

            const std::uint32_t slow = core_a.get_vfp(0);
            const std::uint32_t fast = core_b.get_vfp(0);
            // IXC is excluded: the host fast path trades the INEXACT cumulative
            // flag for speed, uniformly across every operation it handles.
            constexpr std::uint32_t IXC = 0x10u;
            const std::uint32_t slow_fpscr = core_a.get_fpscr() & ~IXC;
            const std::uint32_t fast_fpscr = core_b.get_fpscr() & ~IXC;
            if (slow != fast || slow_fpscr != fast_fpscr) {
                std::printf("[DIVERGENCE] %s host-fast != softfloat seed=%u "
                            "a=%08X n=%08X m=%08X slow=%08X fast=%08X "
                            "slow_fpscr=%08X fast_fpscr=%08X\n",
                    op.name, case_seed, accumulator, multiplicand,
                    multiplier, slow, fast, slow_fpscr, fast_fpscr);
                ++failures;
                if (failures >= 20)
                    break;
            }
        }
    }

    const std::uint64_t hits = vfp_single_host_fast_hits_for_test();
    if (hits == 0) {
        std::printf("[HARNESS BUG] VFP MAC host-fast envelope was never exercised\n");
        ++failures;
    } else {
        std::printf("dyncom_difftest: VFP MAC host-fast %llu hits across %u randomized inputs\n",
            static_cast<unsigned long long>(hits), cases);
    }
    vfp_set_single_host_fast_for_test(true);
    return failures;
}

std::uint32_t benchmark_vfp_mac(diff_env &env_a, diff_env &env_b,
    dyncom_core &core_a, dyncom_core &core_b) {
    constexpr std::uint32_t inst = 0xEE000A81u; // vmla.f32 s0, s1, s2
    constexpr std::uint32_t instructions = 8192;
    constexpr std::uint32_t rounds = 20;
    static_assert(instructions * sizeof(inst) <= MEM_SIZE);

    for (std::uint32_t offset = 0; offset < instructions * sizeof(inst); offset += sizeof(inst)) {
        std::memcpy(env_a.mem.data() + offset, &inst, sizeof(inst));
        std::memcpy(env_b.mem.data() + offset, &inst, sizeof(inst));
    }
    core_a.imb_range(0, instructions * sizeof(inst));
    core_b.imb_range(0, instructions * sizeof(inst));

    auto prepare = [](dyncom_core &core) {
        core.set_cpsr(0x10);
        core.set_pc(0);
        core.set_fpscr(0);
        core.set_vfp(0, 0x3F800000u); // 1.0
        core.set_vfp(1, 0x35800000u); // 2^-20
        core.set_vfp(2, 0x3F800000u); // 1.0
    };
    using clock = std::chrono::steady_clock;
    std::chrono::nanoseconds slow_time{ 0 }, fast_time{ 0 };
    vfp_reset_single_host_fast_hits_for_test();

    for (std::uint32_t round = 0; round < rounds; ++round) {
        prepare(core_a);
        vfp_set_single_host_fast_for_test(false);
        const auto slow_start = clock::now();
        core_a.run(instructions);
        slow_time += clock::now() - slow_start;

        prepare(core_b);
        vfp_set_single_host_fast_for_test(true);
        const auto fast_start = clock::now();
        core_b.run(instructions);
        fast_time += clock::now() - fast_start;
    }

    const std::uint32_t slow = core_a.get_vfp(0);
    const std::uint32_t fast = core_b.get_vfp(0);
    const std::uint64_t hits = vfp_single_host_fast_hits_for_test();
    vfp_set_single_host_fast_for_test(true);
    if (slow != fast || hits != static_cast<std::uint64_t>(instructions) * rounds) {
        std::printf("[HARNESS BUG] VFP MAC benchmark mismatch slow=%08X fast=%08X "
                    "hits=%llu expected=%u\n",
            slow, fast, static_cast<unsigned long long>(hits), instructions * rounds);
        return 1;
    }

    const double slow_ms = std::chrono::duration<double, std::milli>(slow_time).count();
    const double fast_ms = std::chrono::duration<double, std::milli>(fast_time).count();
    std::printf("dyncom_difftest: VFP MAC microbenchmark soft=%.2fms host-fast=%.2fms (%.2fx)\n",
        slow_ms, fast_ms, slow_ms / fast_ms);
    return 0;
}

// A random data-processing instruction. cond is AL most of the time but
// sometimes a real condition (to exercise the conditional path). Operand
// registers are kept out of R15 (PC) so there are no PC-as-operand / PC-write
// edge cases -- those belong in a dedicated edge corpus.
std::uint32_t gen_data_processing(rng &r) {
    const std::uint32_t cond = (r.range(4) == 0) ? r.range(14) : 0xE; // 0..13 or AL
    std::uint32_t opcode = r.range(16);
    const bool is_imm = r.flip();
    bool S = r.flip();
    if (opcode >= 0x8 && opcode <= 0xB) {
        S = true; // TST/TEQ/CMP/CMN always set flags
    }
    const std::uint32_t rn = r.range(15);  // 0..14
    const std::uint32_t rd = r.range(15);  // 0..14 (never PC)

    std::uint32_t op2;
    if (is_imm) {
        op2 = (r.range(16) << 8) | r.range(256); // rotate(4) + imm8(8)
    } else {
        const std::uint32_t rm = r.range(15);     // 0..14
        const std::uint32_t shtype = r.range(4);
        if (r.flip()) {
            // Register-specified shift: bit4 = 1, amount = Rs[11:8] (0..14).
            const std::uint32_t rs = r.range(15);
            op2 = (rs << 8) | (shtype << 5) | (1u << 4) | rm;
        } else {
            const std::uint32_t amount = r.range(32);
            op2 = (amount << 7) | (shtype << 5) | rm; // bit4 = 0 -> immediate shift
        }
    }

    return (cond << 28) | (0u << 26) | ((is_imm ? 1u : 0u) << 25) | (opcode << 21) | ((S ? 1u : 0u) << 20) | (rn << 16) | (rd << 12) | op2;
}

cpu_state gen_state(rng &r) {
    cpu_state s{};
    for (int i = 0; i < 15; i++) {
        s.reg[i] = r.u32();
    }
    s.reg[15] = 0; // PC at the instruction under test
    // USER mode, ARM state, IRQ/FIQ disabled; random NZCV.
    s.cpsr = 0x10 | (0xF0000000u & r.u32());
    return s;
}

// ---------------------------------------------------------------------------
// Load/store (single data transfer: LDR/STR/LDRB/STRB, immediate offset)
// ---------------------------------------------------------------------------
// Two pages, so a block transfer can be made to straddle the boundary at
// 0x2000 -- the only way to exercise the LDM/STM page cursor's re-resolve path.
constexpr std::uint32_t LS_DATA_LO = 0x1000;
constexpr std::uint32_t LS_DATA_HI = 0x3000;
constexpr std::uint32_t LS_PAGE_EDGE = 0x2000;     // page boundary inside the window
constexpr std::uint32_t LS_DATA_BASE = 0x1800;     // base register points here

// Generate a single-data-transfer instruction whose base register is pointed
// into the data window so the access always lands in mapped memory. Word
// accesses keep an aligned offset (unaligned word rotation belongs in a later
// edge corpus). Returns the encoding; sets init.reg[Rn] = base.
std::uint32_t gen_load_store(rng &r, cpu_state &init) {
    const std::uint32_t cond = (r.range(4) == 0) ? r.range(14) : 0xE;
    const std::uint32_t P = r.flip() ? 1 : 0;
    const std::uint32_t U = r.flip() ? 1 : 0;
    const std::uint32_t B = r.flip() ? 1 : 0;            // 1 = byte, 0 = word
    const std::uint32_t W = (P == 1) ? (r.flip() ? 1 : 0) : 0; // post-index: no W (would be LDRT)
    const std::uint32_t L = r.flip() ? 1 : 0;            // 1 = load, 0 = store

    const std::uint32_t rn = r.range(15);                // base, never PC
    std::uint32_t rd = r.range(15);
    while (rd == rn) {
        rd = r.range(15);                                // base==dest writeback is unpredictable
    }

    init.reg[rn] = LS_DATA_BASE;

    const bool reg_off = r.flip();
    if (reg_off) {
        // Scaled-register offset (I=1), LSL-scaled so a word access stays
        // aligned; Rm holds a small value (!= base) so the address stays in the
        // window. This is the classic [Rn, Rm, LSL #k] array-index form.
        std::uint32_t rm = r.range(15);
        while (rm == rn) {
            rm = r.range(15);
        }
        const std::uint32_t shamt = (B == 0) ? 2 : (r.flip() ? 0 : 1); // word: <<2 (aligned)
        init.reg[rm] = r.range(0x40);                    // 0..0x3F -> offset <= 0xFC
        const std::uint32_t off_field = (shamt << 7) | (0u << 5) | rm; // LSL, bit4=0
        return (cond << 28) | (0x1u << 26) | (1u << 25) | (P << 24) | (U << 23) | (B << 22) | (W << 21) | (L << 20) | (rn << 16) | (rd << 12) | off_field;
    }

    std::uint32_t offset = r.range(0x100);               // small immediate, stays in window
    if (B == 0) {
        offset &= ~0x3u;                                 // word: aligned
    }

    return (cond << 28) | (0x1u << 26) | (0u << 25) | (P << 24) | (U << 23) | (B << 22) | (W << 21) | (L << 20) | (rn << 16) | (rd << 12) | offset;
}

// Golden model for single data transfer, operating on a memory image.
cpu_state golden_load_store(std::uint32_t inst, const cpu_state &in, std::uint8_t *mem) {
    cpu_state out = in;
    out.reg[15] = in.reg[15] + 4;

    const std::uint32_t cond = inst >> 28;
    if (!cond_passed(cond, in.cpsr)) {
        return out;
    }

    const std::uint32_t P = (inst >> 24) & 1;
    const std::uint32_t U = (inst >> 23) & 1;
    const std::uint32_t B = (inst >> 22) & 1;
    const std::uint32_t W = (inst >> 21) & 1;
    const std::uint32_t L = (inst >> 20) & 1;
    const std::uint32_t rn = (inst >> 16) & 0xF;
    const std::uint32_t rd = (inst >> 12) & 0xF;

    std::uint32_t offset;
    if ((inst >> 25) & 1) {
        // Scaled-register offset (same shift forms as a data-processing
        // immediate shift, but no carry-out is needed for an address).
        const std::uint32_t rm = in.reg[inst & 0xF];
        const std::uint32_t shtype = (inst >> 5) & 0x3;
        const std::uint32_t shamt = (inst >> 7) & 0x1F;
        switch (shtype) {
        case 0: offset = shamt ? (rm << shamt) : rm; break;                                   // LSL (#0 = Rm)
        case 1: offset = shamt ? (rm >> shamt) : 0; break;                                     // LSR (#0 = #32 -> 0)
        case 2: offset = shamt ? (std::uint32_t)((std::int32_t)rm >> shamt)
                               : ((rm & N_BIT) ? 0xFFFFFFFF : 0); break;                       // ASR (#0 = #32)
        default: offset = shamt ? ((rm >> shamt) | (rm << (32 - shamt)))
                                : (((in.cpsr & C_BIT) ? N_BIT : 0) | (rm >> 1)); break;        // ROR / RRX
        }
    } else {
        offset = inst & 0xFFF;
    }

    const std::uint32_t base = in.reg[rn];
    const std::uint32_t off_applied = U ? (base + offset) : (base - offset);
    const std::uint32_t addr = P ? off_applied : base;

    if (L) {
        std::uint32_t val;
        if (B) {
            val = mem[addr];
        } else {
            std::memcpy(&val, mem + addr, 4);
        }
        out.reg[rd] = val;
    } else {
        if (B) {
            mem[addr] = static_cast<std::uint8_t>(in.reg[rd]);
        } else {
            std::uint32_t v = in.reg[rd];
            std::memcpy(mem + addr, &v, 4);
        }
    }

    if (!P) {
        out.reg[rn] = off_applied; // post-indexed always writes back
    } else if (W) {
        out.reg[rn] = off_applied; // pre-indexed with W
    }

    return out;
}

// ---------------------------------------------------------------------------
// Block transfer (LDM/STM) -- the heaviest user of the inline memory accessors
// ---------------------------------------------------------------------------
// Generate an LDM/STM whose base points into the data window; the register list
// is a non-empty subset of r0..r14 excluding the base (so writeback / base-in-
// list edge cases don't apply) and excluding PC (no branch). S (PSR/user-bank)
// is always 0.
std::uint32_t gen_ldm_stm(rng &r, cpu_state &init) {
    const std::uint32_t cond = (r.range(4) == 0) ? r.range(14) : 0xE;
    const std::uint32_t P = r.flip() ? 1 : 0;
    const std::uint32_t U = r.flip() ? 1 : 0;
    const std::uint32_t W = r.flip() ? 1 : 0;
    const std::uint32_t L = r.flip() ? 1 : 0;
    const std::uint32_t rn = r.range(15); // base, 0..14

    std::uint32_t list = 0;
    while (list == 0) {
        list = (r.u32() & 0x7FFF) & ~(1u << rn); // bits 0..14, exclude base
    }

    // A 15-register transfer spans at most 60 bytes, so parking the base in the
    // middle of a page means every run stays inside it and the cursor's
    // page-crossing branch is never taken. Half the cases put the base within
    // +/-64 bytes of the boundary instead, which makes the run straddle it.
    if (r.flip()) {
        init.reg[rn] = LS_PAGE_EDGE + (static_cast<std::uint32_t>(r.range(33)) - 16) * 4;
    } else {
        init.reg[rn] = LS_DATA_BASE;
    }
    return (cond << 28) | (0x4u << 25) | (P << 24) | (U << 23) | (0u << 22) | (W << 21) | (L << 20) | (rn << 16) | list;
}

cpu_state golden_ldm_stm(std::uint32_t inst, const cpu_state &in, std::uint8_t *mem) {
    cpu_state out = in;
    out.reg[15] = in.reg[15] + 4;

    const std::uint32_t cond = inst >> 28;
    if (!cond_passed(cond, in.cpsr)) {
        return out;
    }

    const std::uint32_t P = (inst >> 24) & 1;
    const std::uint32_t U = (inst >> 23) & 1;
    const std::uint32_t W = (inst >> 21) & 1;
    const std::uint32_t L = (inst >> 20) & 1;
    const std::uint32_t rn = (inst >> 16) & 0xF;
    const std::uint32_t list = inst & 0x7FFF; // bits 0..14 (PC excluded by gen)
    const std::uint32_t n = static_cast<std::uint32_t>(std::popcount(list));
    const std::uint32_t base = in.reg[rn];

    // Lowest-numbered register always goes to the lowest address.
    std::uint32_t addr = U ? (P ? base + 4 : base) : (P ? base - 4 * n : base - 4 * (n - 1));

    for (std::uint32_t i = 0; i < 15; i++) {
        if (!(list & (1u << i))) {
            continue;
        }
        if (L) {
            std::uint32_t v;
            std::memcpy(&v, mem + addr, 4);
            out.reg[i] = v;
        } else {
            std::uint32_t v = in.reg[i];
            std::memcpy(mem + addr, &v, 4);
        }
        addr += 4;
    }

    if (W) {
        out.reg[rn] = U ? base + 4 * n : base - 4 * n;
    }

    return out;
}

// ---------------------------------------------------------------------------
// Halfword / signed transfers (LDRH/STRH/LDRSB/LDRSH, immediate offset)
// ---------------------------------------------------------------------------
std::uint32_t gen_halfword(rng &r, cpu_state &init) {
    const std::uint32_t cond = (r.range(4) == 0) ? r.range(14) : 0xE;
    const std::uint32_t P = r.flip() ? 1 : 0;
    const std::uint32_t U = r.flip() ? 1 : 0;
    const std::uint32_t W = (P == 1) ? (r.flip() ? 1 : 0) : 0;

    std::uint32_t L, S, H;
    switch (r.range(4)) {
    case 0: L = 1; S = 0; H = 1; break; // LDRH
    case 1: L = 0; S = 0; H = 1; break; // STRH
    case 2: L = 1; S = 1; H = 1; break; // LDRSH
    default: L = 1; S = 1; H = 0; break; // LDRSB (byte)
    }

    const std::uint32_t rn = r.range(15);
    std::uint32_t rd = r.range(15);
    while (rd == rn) {
        rd = r.range(15);
    }

    std::uint32_t offset = r.range(0x40);
    if (H == 1) {
        offset &= ~1u; // halfword aligned
    }
    init.reg[rn] = LS_DATA_BASE;

    const std::uint32_t immH = (offset >> 4) & 0xF;
    const std::uint32_t immL = offset & 0xF;
    return (cond << 28) | (0u << 25) | (P << 24) | (U << 23) | (1u << 22) | (W << 21) | (L << 20) | (rn << 16) | (rd << 12) | (immH << 8) | (1u << 7) | (S << 6) | (H << 5) | (1u << 4) | immL;
}

cpu_state golden_halfword(std::uint32_t inst, const cpu_state &in, std::uint8_t *mem) {
    cpu_state out = in;
    out.reg[15] = in.reg[15] + 4;

    const std::uint32_t cond = inst >> 28;
    if (!cond_passed(cond, in.cpsr)) {
        return out;
    }

    const std::uint32_t P = (inst >> 24) & 1;
    const std::uint32_t U = (inst >> 23) & 1;
    const std::uint32_t W = (inst >> 21) & 1;
    const std::uint32_t L = (inst >> 20) & 1;
    const std::uint32_t S = (inst >> 6) & 1;
    const std::uint32_t H = (inst >> 5) & 1;
    const std::uint32_t rn = (inst >> 16) & 0xF;
    const std::uint32_t rd = (inst >> 12) & 0xF;
    const std::uint32_t offset = (((inst >> 8) & 0xF) << 4) | (inst & 0xF);

    const std::uint32_t base = in.reg[rn];
    const std::uint32_t off_applied = U ? (base + offset) : (base - offset);
    const std::uint32_t addr = P ? off_applied : base;

    if (L) {
        if (!H) { // LDRSB
            out.reg[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(mem[addr])));
        } else if (S) { // LDRSH
            std::uint16_t h;
            std::memcpy(&h, mem + addr, 2);
            out.reg[rd] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(h)));
        } else { // LDRH
            std::uint16_t h;
            std::memcpy(&h, mem + addr, 2);
            out.reg[rd] = h;
        }
    } else { // STRH
        std::uint16_t h = static_cast<std::uint16_t>(in.reg[rd]);
        std::memcpy(mem + addr, &h, 2);
    }

    if (!P) {
        out.reg[rn] = off_applied;
    } else if (W) {
        out.reg[rn] = off_applied;
    }

    return out;
}

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------
void dump_state(const char *tag, const cpu_state &s) {
    std::printf("  %s:", tag);
    for (int i = 0; i < 16; i++) {
        std::printf(" r%d=%08X", i, s.reg[i]);
    }
    std::printf(" cpsr=%08X\n", s.cpsr);
}

bool report_mismatch(const char *what, std::uint32_t inst, std::uint32_t seed,
    const cpu_state &expect, const cpu_state &got) {
    std::printf("[DIVERGENCE] %s  inst=%08X seed=%u\n", what, inst, seed);
    dump_state("expect", expect);
    dump_state("got   ", got);
    return false;
}


void push32(std::vector<std::uint8_t> &v, std::uint32_t w) {
    v.push_back(static_cast<std::uint8_t>(w));
    v.push_back(static_cast<std::uint8_t>(w >> 8));
    v.push_back(static_cast<std::uint8_t>(w >> 16));
    v.push_back(static_cast<std::uint8_t>(w >> 24));
}

void push16(std::vector<std::uint8_t> &v, std::uint16_t w) {
    v.push_back(static_cast<std::uint8_t>(w));
    v.push_back(static_cast<std::uint8_t>(w >> 8));
}

// ---------------------------------------------------------------------------
// Program-level differential: dyncom vs dynarmic
// ---------------------------------------------------------------------------
// The golden-model phase above executes ONE instruction per case, which cannot
// reach anything that only exists at block-translation time -- the block cache,
// its direct-mapped L1, the ASID tag, or the loop accelerator. This phase runs
// whole programs and uses dynarmic as an independent oracle instead.
//
// Protocol notes (established by measurement, see the spike commit):
//  * dynarmic checks its tick budget at block granularity, so run(n) executes
//    at least a whole block. Programs therefore end in a `b .` self-branch and
//    get a budget larger than they need; both backends end up spinning on the
//    same PC with identical state.
//  * The backends disagree on whether the Thumb bit is folded into r15 while
//    agreeing on CPSR bit 5. Bit 0 is never part of a Thumb address, so it is
//    normalised away.
//  * FPSCR is compared with IXC masked out: the VFP host fast path documents
//    that it does not raise the inexact flag.
//  * dynarmic hands some instructions to its own embedded dyncom. Such a case
//    would be dyncom-vs-dyncom, so the fallback counter must read zero.

constexpr std::uint32_t ARENA_MEM_SIZE = 0x40000; // 256 KB
constexpr std::uint32_t PROG_DATA_LO = 0x8000;    // two pages of data
constexpr std::uint32_t PROG_DATA_HI = 0xA000;
constexpr std::uint32_t PROG_DATA_PTR = 0x9000;   // pointer regs start on the page edge
constexpr std::uint32_t PROG_CODE_LO = 0x10000;
// Entry blocks are indexed by `pc & 2047` (block_l1_index with asid 0), so a
// 2048-byte stride makes every arena program collide in the same L1 slot.
constexpr std::uint32_t PROG_CODE_STRIDE = 2048;

struct backend_env {
    std::vector<std::uint8_t> mem;
    std::unique_ptr<exclusive_monitor> monitor;
    std::unique_ptr<core> cpu;
    const char *name;
    std::uint64_t exceptions = 0;
    exception_type last_exception = exception_type_unk;
    std::uint32_t last_exception_pc = 0;

    backend_env()
        : mem(ARENA_MEM_SIZE, 0) {
    }
};

void wire_backend(backend_env &e) {
    std::uint8_t *base = e.mem.data();
    core *cp = e.cpu.get();

    auto seed_tlb = [cp, base](std::uint32_t addr) {
        const std::uint32_t page = addr & ~PAGE_MASK;
        cp->set_tlb_page(page, base + page, prot_read_write_exec);
    };

    cp->read_code = [base, seed_tlb](const address a, std::uint32_t *r) -> bool {
        if (a + 4 > ARENA_MEM_SIZE) return false;
        std::memcpy(r, base + a, 4);
        seed_tlb(a);
        return true;
    };
    cp->read_8bit = [base, seed_tlb](const address a, std::uint8_t *r) -> bool {
        if (a >= ARENA_MEM_SIZE) return false;
        *r = base[a]; seed_tlb(a); return true;
    };
    cp->read_16bit = [base, seed_tlb](const address a, std::uint16_t *r) -> bool {
        if (a + 2 > ARENA_MEM_SIZE) return false;
        std::memcpy(r, base + a, 2); seed_tlb(a); return true;
    };
    cp->read_32bit = [base, seed_tlb](const address a, std::uint32_t *r) -> bool {
        if (a + 4 > ARENA_MEM_SIZE) return false;
        std::memcpy(r, base + a, 4); seed_tlb(a); return true;
    };
    cp->read_64bit = [base, seed_tlb](const address a, std::uint64_t *r) -> bool {
        if (a + 8 > ARENA_MEM_SIZE) return false;
        std::memcpy(r, base + a, 8); seed_tlb(a); return true;
    };
    cp->write_8bit = [base, seed_tlb](const address a, std::uint8_t *v) -> bool {
        if (a >= ARENA_MEM_SIZE) return false;
        base[a] = *v; seed_tlb(a); return true;
    };
    cp->write_16bit = [base, seed_tlb](const address a, std::uint16_t *v) -> bool {
        if (a + 2 > ARENA_MEM_SIZE) return false;
        std::memcpy(base + a, v, 2); seed_tlb(a); return true;
    };
    cp->write_32bit = [base, seed_tlb](const address a, std::uint32_t *v) -> bool {
        if (a + 4 > ARENA_MEM_SIZE) return false;
        std::memcpy(base + a, v, 4); seed_tlb(a); return true;
    };
    cp->write_64bit = [base, seed_tlb](const address a, std::uint64_t *v) -> bool {
        if (a + 8 > ARENA_MEM_SIZE) return false;
        std::memcpy(base + a, v, 8); seed_tlb(a); return true;
    };

    backend_env *ep = &e;
    cp->exception_handler = [ep](exception_type t, const std::uint32_t pc) -> bool {
        ep->exceptions++;
        ep->last_exception = t;
        ep->last_exception_pc = pc;
        return false;
    };
    cp->system_call_handler = [](const std::uint32_t) {};
}

std::unique_ptr<backend_env> make_dyncom_env() {
    auto e = std::make_unique<backend_env>();
    e->name = "dyncom";
    e->monitor = std::make_unique<r12l1::exclusive_monitor>(1);
    e->cpu = std::make_unique<dyncom_core>(e->monitor.get(), PAGE_BITS);
    wire_backend(*e);
    return e;
}

std::unique_ptr<backend_env> make_dynarmic_env() {
    auto e = std::make_unique<backend_env>();
    e->name = "dynarmic";
    // dynarmic_core reinterpret_casts the monitor to its own type, so the two
    // backends cannot share one.
    e->monitor = std::make_unique<dynarmic_exclusive_monitor>(1);
    e->cpu = std::make_unique<dynarmic_core>(e->monitor.get());
    wire_backend(*e);
    return e;
}

// A generated test program: an image, where it lives, how it starts.
struct program {
    std::vector<std::uint8_t> code;
    std::uint32_t addr = PROG_CODE_LO;
    bool thumb = false;
    std::uint32_t init[16] = {};
    std::uint32_t vfp_init[32] = {};
    std::uint32_t budget = 4096;
    const char *kind = "arm";
};

struct full_state {
    std::uint32_t reg[16];
    std::uint32_t cpsr;
    std::uint32_t vfp[32];
    std::uint32_t fpscr;
};

// IXC (bit 4) is excluded: the VFP host fast path documents that it does not
// raise the inexact cumulative flag.
constexpr std::uint32_t FPSCR_COMPARE_MASK = ~0x10u;

full_state capture(core &c) {
    full_state s{};
    for (int i = 0; i < 16; i++) s.reg[i] = c.get_reg(i);
    s.cpsr = c.get_cpsr();
    if (s.cpsr & 0x20) s.reg[15] &= ~1u;
    for (int i = 0; i < 32; i++) s.vfp[i] = c.get_vfp(i);
    s.fpscr = c.get_fpscr() & FPSCR_COMPARE_MASK;
    return s;
}

void seed_data_window(std::vector<std::uint8_t> &mem, std::uint32_t seed) {
    rng r(seed);
    for (std::uint32_t a = PROG_DATA_LO; a < PROG_DATA_HI; a += 4) {
        const std::uint32_t w = r.u32();
        std::memcpy(mem.data() + a, &w, 4);
    }
}

// Install a program image without touching the translation caches. `warm`
// keeps whatever the backend has cached, which is what makes block-cache and
// L1 behaviour observable at all.
void install(backend_env &e, const program &p) {
    std::memcpy(e.mem.data() + p.addr, p.code.data(), p.code.size());
}

full_state execute(backend_env &e, const program &p) {
    for (int i = 0; i < 15; i++) e.cpu->set_reg(i, p.init[i]);
    for (int i = 0; i < 32; i++) e.cpu->set_vfp(i, p.vfp_init[i]);
    e.cpu->set_fpscr(0);
    e.cpu->set_cpsr(p.thumb ? (0x10u | 0x20u) : 0x10u);
    e.cpu->set_pc(p.addr | (p.thumb ? 1u : 0u));
    e.cpu->run(p.budget);
    return capture(*e.cpu);
}

bool states_equal(const full_state &a, const full_state &b) {
    if (std::memcmp(a.reg, b.reg, sizeof(a.reg)) != 0) return false;
    if (a.cpsr != b.cpsr) return false;
    if (std::memcmp(a.vfp, b.vfp, sizeof(a.vfp)) != 0) return false;
    return a.fpscr == b.fpscr;
}

void dump_full(const char *tag, const full_state &s) {
    std::printf("  %-9s cpsr=%08X fpscr=%08X\n", tag, s.cpsr, s.fpscr);
    std::printf("           ");
    for (int i = 0; i < 16; i++) std::printf(" r%d=%08X", i, s.reg[i]);
    std::printf("\n");
}

// Re-run growing prefixes of a diverging program to find the first instruction
// whose result differs. Without this, a divergence is a wall of registers and a
// program the reader has to disassemble by hand.
void reduce_divergence(backend_env &dc, backend_env &da, const program &p, std::uint32_t seed) {
    const std::uint32_t step = p.thumb ? 2u : 4u;
    const std::uint32_t body = static_cast<std::uint32_t>(p.code.size()) - step;

    for (std::uint32_t n = step; n <= body; n += step) {
        program q = p;
        q.code.assign(p.code.begin(), p.code.begin() + n);
        if (p.thumb) push16(q.code, 0xE7FEu);
        else push32(q.code, 0xEAFFFFFEu);
        q.budget = n + 64;

        seed_data_window(dc.mem, seed);
        seed_data_window(da.mem, seed);
        install(dc, q);
        install(da, q);
        dc.cpu->clear_instruction_cache();
        da.cpu->clear_instruction_cache();
        dc.cpu->imb_range(q.addr, q.code.size());
        da.cpu->imb_range(q.addr, q.code.size());

        const full_state sd = execute(dc, q);
        const full_state sa = execute(da, q);
        const bool mem_ok = std::memcmp(dc.mem.data() + PROG_DATA_LO,
                                da.mem.data() + PROG_DATA_LO,
                                PROG_DATA_HI - PROG_DATA_LO)
            == 0;
        if (!states_equal(sd, sa) || !mem_ok) {
            std::uint32_t inst = 0;
            std::memcpy(&inst, p.code.data() + (n - step), step);
            std::printf("  first divergence at instruction %u: %0*X\n",
                (n / step) - 1, static_cast<int>(step * 2), inst);
            for (int i = 0; i < 16; i++) {
                if (sd.reg[i] != sa.reg[i])
                    std::printf("    r%-2d dyncom=%08X dynarmic=%08X\n", i, sd.reg[i], sa.reg[i]);
            }
            if (sd.cpsr != sa.cpsr)
                std::printf("    cpsr dyncom=%08X dynarmic=%08X\n", sd.cpsr, sa.cpsr);
            for (int i = 0; i < 32; i++) {
                if (sd.vfp[i] != sa.vfp[i])
                    std::printf("    s%-2d dyncom=%08X dynarmic=%08X\n", i, sd.vfp[i], sa.vfp[i]);
            }
            if (sd.fpscr != sa.fpscr)
                std::printf("    fpscr dyncom=%08X dynarmic=%08X\n", sd.fpscr, sa.fpscr);
            if (!mem_ok) std::printf("    (memory differs)\n");
            return;
        }
    }
    std::printf("  no single-instruction prefix reproduces it: the divergence needs "
                "the warm translation cache\n");
}

bool report_program_divergence(const char *suite, const program &p, std::uint32_t seed,
    const full_state &dc, const full_state &da,
    const std::vector<std::uint8_t> &mdc, const std::vector<std::uint8_t> &mda) {
    std::printf("[DIVERGENCE] %s (%s) seed=%u addr=%08X %s\n", suite, p.kind, seed,
        p.addr, p.thumb ? "thumb" : "arm");
    dump_full("dyncom", dc);
    dump_full("dynarmic", da);
    for (std::uint32_t a = PROG_DATA_LO; a < PROG_DATA_HI; a += 4) {
        std::uint32_t x, y;
        std::memcpy(&x, mdc.data() + a, 4);
        std::memcpy(&y, mda.data() + a, 4);
        if (x != y) {
            std::printf("  mem[%08X] dyncom=%08X dynarmic=%08X\n", a, x, y);
            break;
        }
    }
    std::printf("  code:");
    for (std::size_t i = 0; i < p.code.size(); i += (p.thumb ? 2 : 4)) {
        std::uint32_t w = 0;
        std::memcpy(&w, p.code.data() + i, p.thumb ? 2 : 4);
        std::printf(p.thumb ? " %04X" : " %08X", w);
    }
    std::printf("\n");
    return false;
}

// ---------------------------------------------------------------------------
// Program generators
// ---------------------------------------------------------------------------
// Only well-defined encodings are emitted. dyncom and dynarmic are both free to
// do whatever they like with UNPREDICTABLE forms, so generating them would
// produce divergences that mean nothing. The register assignment below is what
// keeps every generated access inside the mapped data window without having to
// simulate the program first.

// ARM: r9/r10 hold data pointers, r7 holds a small scaled-index value; none of
// the three is ever a destination, so the pointers stay in the window even with
// base write-back (bounded to +/-32 per access, well under the window margin).
constexpr int A_PTR1 = 9;
constexpr int A_PTR2 = 10;
constexpr int A_OFF = 7;
constexpr std::uint32_t A_WRITABLE[] = { 0, 1, 2, 3, 4, 5, 6, 8, 11, 12, 13, 14 };
constexpr std::uint32_t A_WRITABLE_N = sizeof(A_WRITABLE) / sizeof(A_WRITABLE[0]);

std::uint32_t a_wreg(rng &r) { return A_WRITABLE[r.range(A_WRITABLE_N)]; }
std::uint32_t a_anyreg(rng &r) {
    const std::uint32_t pick = r.range(A_WRITABLE_N + 3);
    if (pick < A_WRITABLE_N) return A_WRITABLE[pick];
    return (pick == A_WRITABLE_N) ? A_PTR1 : ((pick == A_WRITABLE_N + 1) ? A_PTR2 : A_OFF);
}
std::uint32_t a_cond(rng &r) { return (r.range(4) == 0) ? r.range(14) : 0xEu; }

std::uint32_t gen_arm_data_processing(rng &r) {
    std::uint32_t opcode = r.range(16);
    std::uint32_t s = r.flip() ? 1u : 0u;
    if (opcode >= 8 && opcode <= 11) s = 1; // TST/TEQ/CMP/CMN always set flags
    const std::uint32_t rd = (opcode >= 8 && opcode <= 11) ? 0u : a_wreg(r);
    // MOV and MVN ignore Rn and require the field to be zero; TST/TEQ/CMP/CMN
    // likewise require Rd to be zero (handled above). A non-zero SBZ field is
    // UNPREDICTABLE, and the backends legitimately differ there -- dynarmic
    // rejects it as undefined while dyncom executes it.
    const bool rn_sbz = (opcode == 13) || (opcode == 15);
    const std::uint32_t rn = rn_sbz ? 0u : a_anyreg(r);

    std::uint32_t i_bit = 0, operand2 = 0;
    switch (r.range(3)) {
    case 0: // #imm rotated
        i_bit = 1;
        operand2 = (r.range(16) << 8) | r.range(256);
        break;
    case 1: // Rm shifted by an immediate
        operand2 = (r.range(32) << 7) | (r.range(4) << 5) | a_anyreg(r);
        break;
    default: // Rm shifted by Rs
        operand2 = (a_anyreg(r) << 8) | (r.range(4) << 5) | (1u << 4) | a_anyreg(r);
        break;
    }
    return (a_cond(r) << 28) | (i_bit << 25) | (opcode << 21) | (s << 20) | (rn << 16)
        | (rd << 12) | operand2;
}

std::uint32_t gen_arm_multiply(rng &r) {
    const std::uint32_t rd = a_wreg(r);
    std::uint32_t rm = a_anyreg(r);
    if (rm == rd) rm = (rd == A_PTR1) ? A_PTR2 : A_PTR1; // Rd == Rm is UNPREDICTABLE
    const std::uint32_t rs = a_anyreg(r);
    const std::uint32_t s = r.flip() ? 1u : 0u;
    if (r.flip()) { // MLA
        const std::uint32_t rn = a_anyreg(r);
        return (a_cond(r) << 28) | (1u << 21) | (s << 20) | (rd << 16) | (rn << 12)
            | (rs << 8) | (9u << 4) | rm;
    }
    return (a_cond(r) << 28) | (s << 20) | (rd << 16) | (rs << 8) | (9u << 4) | rm;
}

std::uint32_t gen_arm_single_transfer(rng &r) {
    const std::uint32_t p = r.flip() ? 1u : 0u;
    const std::uint32_t u = r.flip() ? 1u : 0u;
    const std::uint32_t b = r.flip() ? 1u : 0u;
    const std::uint32_t l = r.flip() ? 1u : 0u;
    // P == 0 already writes back; W == 1 there would mean the unprivileged
    // LDRT/STRT forms, which is a different instruction.
    const std::uint32_t w = p ? (r.flip() ? 1u : 0u) : 0u;
    const std::uint32_t rn = r.flip() ? A_PTR1 : A_PTR2;
    const std::uint32_t rt = a_wreg(r);

    std::uint32_t i_bit = 0, offset = 0;
    if (r.flip()) {
        // Immediate: the magnitude is small enough that write-back cannot walk
        // the pointer out of the window. Any form that writes the base back
        // keeps the offset word-aligned, otherwise a later word access would
        // become unaligned -- where the backends legally differ (rotate the
        // loaded word vs perform a true unaligned access).
        const bool writes_base = (w != 0) || (p == 0);
        offset = (b && !writes_base) ? r.range(33) : (r.range(9) * 4);
    } else {
        // Scaled register: the index register holds a small value.
        i_bit = 1;
        offset = (r.range(3) << 7) | (0u << 5) | A_OFF; // LSL #0..2
    }
    return (a_cond(r) << 28) | (1u << 26) | (i_bit << 25) | (p << 24) | (u << 23)
        | (b << 22) | (w << 21) | (l << 20) | (rn << 16) | (rt << 12) | offset;
}

std::uint32_t gen_arm_halfword(rng &r) {
    const std::uint32_t p = r.flip() ? 1u : 0u;
    const std::uint32_t u = r.flip() ? 1u : 0u;
    const std::uint32_t w = p ? (r.flip() ? 1u : 0u) : 0u;
    const std::uint32_t rn = r.flip() ? A_PTR1 : A_PTR2;
    const std::uint32_t rt = a_wreg(r);

    std::uint32_t l, sbit, h;
    if (r.flip()) { // STRH
        l = 0; sbit = 0; h = 1;
    } else {
        l = 1;
        switch (r.range(3)) {
        case 0: sbit = 0; h = 1; break;  // LDRH
        case 1: sbit = 1; h = 0; break;  // LDRSB
        default: sbit = 1; h = 1; break; // LDRSH
        }
    }

    std::uint32_t i_bit, offset_field;
    if (r.flip()) {
        i_bit = 1;
        const bool writes_base = (w != 0) || (p == 0);
        const std::uint32_t off = writes_base ? (r.range(9) * 4) : (r.range(17) * 2);
        offset_field = ((off & 0xF0u) << 4) | (off & 0x0Fu);
    } else {
        i_bit = 0;
        offset_field = A_OFF;
    }
    return (a_cond(r) << 28) | (p << 24) | (u << 23) | (i_bit << 22) | (w << 21)
        | (l << 20) | (rn << 16) | (rt << 12) | offset_field | (1u << 7)
        | (sbit << 6) | (h << 5) | (1u << 4);
}

std::uint32_t gen_arm_block_transfer(rng &r, bool straddle_page) {
    const std::uint32_t p = r.flip() ? 1u : 0u;
    const std::uint32_t u = r.flip() ? 1u : 0u;
    const std::uint32_t w = r.flip() ? 1u : 0u;
    const std::uint32_t l = r.flip() ? 1u : 0u;
    const std::uint32_t rn = straddle_page ? A_PTR1 : A_PTR2;

    std::uint32_t list = 0;
    while (list == 0) {
        for (std::uint32_t i = 0; i < A_WRITABLE_N; i++) {
            if (r.flip()) list |= 1u << A_WRITABLE[i];
        }
    }
    return (a_cond(r) << 28) | (4u << 25) | (p << 24) | (u << 23) | (w << 21)
        | (l << 20) | (rn << 16) | list;
}

// Thumb: r5 is the data pointer and r6 a small index; neither is ever written,
// and the low-register forms have no write-back, so no drift is possible.
constexpr int T_PTR = 5;
constexpr int T_OFF = 6;
constexpr std::uint32_t T_WRITABLE[] = { 0, 1, 2, 3, 4, 7 };
constexpr std::uint32_t T_WRITABLE_N = sizeof(T_WRITABLE) / sizeof(T_WRITABLE[0]);

std::uint32_t t_wreg(rng &r) { return T_WRITABLE[r.range(T_WRITABLE_N)]; }
std::uint32_t t_anyreg(rng &r) {
    const std::uint32_t pick = r.range(T_WRITABLE_N + 2);
    if (pick < T_WRITABLE_N) return T_WRITABLE[pick];
    return (pick == T_WRITABLE_N) ? T_PTR : T_OFF;
}

std::uint16_t gen_thumb_inst(rng &r) {
    switch (r.range(8)) {
    case 0: { // LSL/LSR/ASR by immediate
        const std::uint32_t op = r.range(3);
        return static_cast<std::uint16_t>((op << 11) | (r.range(32) << 6)
            | (t_anyreg(r) << 3) | t_wreg(r));
    }
    case 1: { // ADD/SUB register or 3-bit immediate
        const std::uint32_t i_bit = r.flip() ? 1u : 0u;
        const std::uint32_t op = r.flip() ? 1u : 0u;
        const std::uint32_t rn = i_bit ? r.range(8) : t_anyreg(r);
        return static_cast<std::uint16_t>(0x1800u | (i_bit << 10) | (op << 9)
            | (rn << 6) | (t_anyreg(r) << 3) | t_wreg(r));
    }
    case 2: { // MOV/CMP/ADD/SUB 8-bit immediate
        const std::uint32_t op = r.range(4);
        const std::uint32_t rd = (op == 1) ? t_anyreg(r) : t_wreg(r); // CMP writes nothing
        return static_cast<std::uint16_t>(0x2000u | (op << 11) | (rd << 8) | r.range(256));
    }
    case 3: { // ALU operations
        const std::uint32_t op = r.range(16);
        const std::uint32_t rd = (op == 8 || op == 10 || op == 11) // TST/CMP/CMN
            ? t_anyreg(r) : t_wreg(r);
        return static_cast<std::uint16_t>(0x4000u | (op << 6) | (t_anyreg(r) << 3) | rd);
    }
    case 4: { // High-register ADD/CMP/MOV; never touches SP or PC
        const std::uint32_t op = r.range(3);
        const std::uint32_t rd = 8 + r.range(5);  // r8..r12
        const std::uint32_t rm = t_anyreg(r);
        return static_cast<std::uint16_t>(0x4400u | (op << 8) | (1u << 7)
            | (rm << 3) | (rd & 7));
    }
    case 5: { // Register-offset load/store
        const std::uint32_t op = r.range(8);
        return static_cast<std::uint16_t>(0x5000u | (op << 9) | (T_OFF << 6)
            | (T_PTR << 3) | t_wreg(r));
    }
    case 6: { // Word / byte immediate-offset load/store
        const std::uint32_t b = r.flip() ? 1u : 0u;
        const std::uint32_t l = r.flip() ? 1u : 0u;
        const std::uint32_t off = b ? r.range(32) : r.range(24);
        return static_cast<std::uint16_t>(0x6000u | (b << 12) | (l << 11)
            | (off << 6) | (T_PTR << 3) | t_wreg(r));
    }
    default: { // Halfword immediate-offset load/store
        const std::uint32_t l = r.flip() ? 1u : 0u;
        return static_cast<std::uint16_t>(0x8000u | (l << 11) | (r.range(24) << 6)
            | (T_PTR << 3) | t_wreg(r));
    }
    }
}

void set_common_init(program &p, rng &r) {
    for (int i = 0; i < 15; i++) p.init[i] = r.u32();
    if (p.thumb) {
        p.init[T_PTR] = PROG_DATA_PTR;
        p.init[T_OFF] = r.range(16) * 4;
    } else {
        p.init[A_PTR1] = PROG_DATA_PTR;
        p.init[A_PTR2] = PROG_DATA_PTR + 0x400;
        p.init[A_OFF] = r.range(16) * 4;
    }
}

program gen_arm_program(rng &r, std::uint32_t addr, std::uint32_t length) {
    program p;
    p.addr = addr;
    p.thumb = false;
    p.kind = "arm-mixed";
    set_common_init(p, r);

    // Triage aid: restrict generation to one instruction class so a divergence
    // can be attributed without hand-decoding a whole program.
    static const char *only = std::getenv("EKA2L1_DIFFTEST_ARM_ONLY");

    for (std::uint32_t i = 0; i < length; i++) {
        std::uint32_t inst;
        if (only) {
            if (std::strcmp(only, "dp") == 0) inst = gen_arm_data_processing(r);
            else if (std::strcmp(only, "ls") == 0) inst = gen_arm_single_transfer(r);
            else if (std::strcmp(only, "hw") == 0) inst = gen_arm_halfword(r);
            else if (std::strcmp(only, "blk") == 0) inst = gen_arm_block_transfer(r, r.flip());
            else inst = gen_arm_multiply(r);
            push32(p.code, inst);
            continue;
        }
        switch (r.range(10)) {
        case 0: case 1: case 2: case 3:
            inst = gen_arm_data_processing(r);
            break;
        case 4: case 5:
            inst = gen_arm_single_transfer(r);
            break;
        case 6:
            inst = gen_arm_halfword(r);
            break;
        case 7: case 8:
            // Half of the block transfers use the pointer sitting on the page
            // edge, so the LDM/STM page cursor has to re-resolve mid-run.
            inst = gen_arm_block_transfer(r, r.flip());
            break;
        default:
            inst = gen_arm_multiply(r);
            break;
        }
        push32(p.code, inst);
    }
    push32(p.code, 0xEAFFFFFEu); // b .
    p.budget = length * 4 + 64;
    return p;
}

program gen_thumb_program(rng &r, std::uint32_t addr, std::uint32_t length) {
    program p;
    p.addr = addr;
    p.thumb = true;
    p.kind = "thumb-mixed";
    set_common_init(p, r);

    for (std::uint32_t i = 0; i < length; i++) {
        push16(p.code, gen_thumb_inst(r));
    }
    push16(p.code, 0xE7FEu); // b .
    p.budget = length * 4 + 64;
    return p;
}

// ---------------------------------------------------------------------------
// Differential suites
// ---------------------------------------------------------------------------

struct coverage {
    std::uint64_t programs = 0;
    std::uint64_t fallback_insts = 0;
    std::uint64_t accel_attaches = 0;
    std::uint64_t accel_bulk_iterations = 0;
};

// Run one program on both backends and compare everything guest-visible.
// `reset_caches` is what separates a cold case from an arena case: leaving the
// caches warm is the only way block reuse, the L1 and the ASID tag can be
// observed at all.
bool run_pair(const char *suite, backend_env &dc, backend_env &da, const program &p,
    std::uint32_t seed, bool reset_caches, coverage &cov) {
    seed_data_window(dc.mem, seed);
    seed_data_window(da.mem, seed);
    install(dc, p);
    install(da, p);

    if (reset_caches) {
        dc.cpu->clear_instruction_cache();
        da.cpu->clear_instruction_cache();
        dc.cpu->imb_range(p.addr, p.code.size());
        da.cpu->imb_range(p.addr, p.code.size());
    }

    dynarmic_reset_interpreter_fallback_for_test();
    dyncom_reset_loop_accel_counters_for_test();
    dc.exceptions = 0;
    da.exceptions = 0;

    const full_state sd = execute(dc, p);
    cov.accel_attaches += dyncom_loop_accel_attaches_for_test();
    cov.accel_bulk_iterations += dyncom_loop_accel_bulk_iterations_for_test();

    dynarmic_reset_interpreter_fallback_for_test();
    const full_state sa = execute(da, p);
    const std::uint64_t fallback = dynarmic_interpreter_fallback_for_test();
    cov.fallback_insts += fallback;
    cov.programs++;

    if (dc.exceptions != 0 || da.exceptions != 0) {
        // Programs are generated to be well-defined and fault-free. An exception
        // means the generator emitted an encoding it should not have -- comparing
        // the two backends past that point is meaningless, since they are free to
        // handle UNDEFINED/UNPREDICTABLE forms differently.
        std::printf("[HARNESS BUG] %s: guest exception (dyncom=%llu type=%d pc=%08X, "
                    "dynarmic=%llu type=%d pc=%08X) addr=%08X seed=%u\n",
            suite, static_cast<unsigned long long>(dc.exceptions),
            static_cast<int>(dc.last_exception), dc.last_exception_pc,
            static_cast<unsigned long long>(da.exceptions),
            static_cast<int>(da.last_exception), da.last_exception_pc, p.addr, seed);
        return false;
    }

    if (fallback != 0) {
        // dynarmic executed part of this program with its own embedded dyncom,
        // so agreement here would be dyncom-vs-dyncom. Report rather than pass.
        std::printf("[HARNESS BUG] %s: dynarmic fell back to its interpreter for "
                    "%llu instruction(s) at addr=%08X seed=%u -- the oracle is not "
                    "independent for this case\n",
            suite, static_cast<unsigned long long>(fallback), p.addr, seed);
        return false;
    }

    const bool mem_ok = std::memcmp(dc.mem.data() + PROG_DATA_LO,
                            da.mem.data() + PROG_DATA_LO, PROG_DATA_HI - PROG_DATA_LO)
        == 0;
    if (!states_equal(sd, sa) || !mem_ok) {
        report_program_divergence(suite, p, seed, sd, sa, dc.mem, da.mem);
        reduce_divergence(dc, da, p, seed);
        return false;
    }
    return true;
}

// Cold cases: every program gets a flushed translation cache, so this isolates
// instruction semantics -- the ALU, shifter, addressing modes, the inline
// memory accessors, the LDM/STM cursor and the Thumb paths.
std::uint32_t suite_cold(backend_env &dc, backend_env &da, std::uint32_t base_seed,
    std::uint32_t count, coverage &cov) {
    std::uint32_t failures = 0;
    for (std::uint32_t i = 0; i < count && failures < 10; i++) {
        const std::uint32_t seed = base_seed + i;
        rng r(seed);
        const bool thumb = r.flip();
        const std::uint32_t len = 4 + r.range(20);
        const program p = thumb ? gen_thumb_program(r, PROG_CODE_LO, len)
                                : gen_arm_program(r, PROG_CODE_LO, len);
        if (!run_pair("cold", dc, da, p, seed, true, cov)) failures++;
    }
    return failures;
}

// Arena: many programs resident at once, laid out so their entry blocks all
// collide in the same direct-mapped L1 slot, executed in shuffled order with
// the caches left warm. This is what makes a block-cache or L1 tagging bug
// observable; a cold, single-address harness cannot see one.
std::uint32_t suite_arena(backend_env &dc, backend_env &da, std::uint32_t base_seed,
    std::uint32_t rounds, coverage &cov) {
    constexpr std::uint32_t PROGRAMS = 24;
    std::vector<program> progs;
    progs.reserve(PROGRAMS);

    for (std::uint32_t i = 0; i < PROGRAMS; i++) {
        rng r(base_seed ^ (0x1000193u * (i + 1)));
        const std::uint32_t addr = PROG_CODE_LO + i * PROG_CODE_STRIDE;
        const bool thumb = (i & 1) != 0;
        program p = thumb ? gen_thumb_program(r, addr, 6 + r.range(10))
                          : gen_arm_program(r, addr, 6 + r.range(10));
        p.kind = thumb ? "arena-thumb" : "arena-arm";
        progs.push_back(std::move(p));
    }

    // Install everything and invalidate once; from here the caches stay warm.
    for (const program &p : progs) {
        install(dc, p);
        install(da, p);
    }
    dc.cpu->clear_instruction_cache();
    da.cpu->clear_instruction_cache();

    std::uint32_t failures = 0;
    for (std::uint32_t round = 0; round < rounds && failures < 10; round++) {
        rng order(base_seed + 0x51ED2701u + round);
        for (std::uint32_t n = 0; n < PROGRAMS; n++) {
            const program &p = progs[order.range(PROGRAMS)];
            if (!run_pair("arena", dc, da, p, base_seed + round, false, cov)) {
                failures++;
                break;
            }
        }
    }
    return failures;
}

// ASID: the same virtual address backed by different code in two address
// spaces. dyncom is told about the switch through set_asid and must keep the
// two translations apart without flushing; dynarmic has no ASID concept, so it
// is flushed on every switch and is therefore the ground truth.
std::uint32_t suite_asid(backend_env &dc, backend_env &da, std::uint32_t base_seed,
    std::uint32_t rounds, coverage &cov) {
    constexpr std::uint32_t VA = PROG_CODE_LO + 0x4000;

    program space[2];
    for (int i = 0; i < 2; i++) {
        rng r(base_seed ^ (0xB5297A4Du * static_cast<std::uint32_t>(i + 1)));
        space[i] = gen_arm_program(r, VA, 8 + r.range(8));
        space[i].kind = (i == 0) ? "asid-space-0" : "asid-space-1";
        // Both spaces start from the same registers so a divergence can only
        // come from executing the wrong image.
        rng shared(base_seed);
        set_common_init(space[i], shared);
    }

    dc.cpu->clear_instruction_cache();
    da.cpu->clear_instruction_cache();

    std::uint32_t failures = 0;
    for (std::uint32_t round = 0; round < rounds && failures < 10; round++) {
        const int which = static_cast<int>(round & 1);
        const std::uint32_t asid = 1u + static_cast<std::uint32_t>(which);

        // Swap in this space's image WITHOUT invalidating dyncom's cache: a
        // correctly ASID-tagged cache keeps one translation per space and must
        // pick the right one. dynarmic is flushed so it always re-translates.
        dc.cpu->set_asid(asid);
        da.cpu->clear_instruction_cache();

        if (!run_pair("asid", dc, da, space[which], base_seed + round, false, cov)) {
            failures++;
        }
    }
    dc.cpu->set_asid(0);
    return failures;
}

// Loop accelerator: the canonical shapes its translation-time matcher accepts.
// Run to completion so the bulk step actually executes, and assert afterwards
// that it attached at all -- a matcher that silently stopped matching would
// otherwise look like a clean pass.
std::uint32_t suite_loops(backend_env &dc, backend_env &da, std::uint32_t base_seed,
    std::uint32_t count, coverage &cov) {
    std::uint32_t failures = 0;

    for (std::uint32_t i = 0; i < count && failures < 10; i++) {
        const std::uint32_t seed = base_seed + i;
        rng r(seed);

        program p;
        p.addr = PROG_CODE_LO;
        p.thumb = true;
        set_common_init(p, r);

        const std::uint32_t iterations = 3 + r.range(120);
        p.init[1] = PROG_DATA_PTR - 0x800;             // source
        p.init[2] = PROG_DATA_PTR - 0x800 + 0x800;     // destination
        p.init[3] = iterations;
        p.init[T_PTR] = PROG_DATA_PTR;
        p.init[T_OFF] = 0;

        const std::uint32_t shape = r.range(4);
        switch (shape) {
        case 0: // word copy
            p.kind = "loop-word-copy";
            push16(p.code, 0x6808); // ldr  r0, [r1]
            push16(p.code, 0x6010); // str  r0, [r2]
            push16(p.code, 0x3104); // adds r1, #4
            push16(p.code, 0x3204); // adds r2, #4
            break;
        case 1: // halfword copy
            p.kind = "loop-halfword-copy";
            push16(p.code, 0x8808); // ldrh r0, [r1]
            push16(p.code, 0x8010); // strh r0, [r2]
            push16(p.code, 0x3102); // adds r1, #2
            push16(p.code, 0x3202); // adds r2, #2
            break;
        case 2: // byte copy
            p.kind = "loop-byte-copy";
            push16(p.code, 0x7808); // ldrb r0, [r1]
            push16(p.code, 0x7010); // strb r0, [r2]
            push16(p.code, 0x3101); // adds r1, #1
            push16(p.code, 0x3201); // adds r2, #1
            break;
        default: // shift/mask convert: 32bpp -> 16bpp style
            p.kind = "loop-convert";
            push16(p.code, 0x6808); // ldr  r0, [r1]
            push16(p.code, 0x0A00); // lsrs r0, r0, #8
            push16(p.code, 0x8010); // strh r0, [r2]
            push16(p.code, 0x3104); // adds r1, #4
            push16(p.code, 0x3202); // adds r2, #2
            break;
        }
        push16(p.code, 0x3B01); // subs r3, #1
        // Branch back to the top of the body.
        // Thumb B<cond> is relative to (branch address + 4), i.e. two halfwords
        // past the branch itself.
        const std::uint32_t body_halfwords = static_cast<std::uint32_t>(p.code.size() / 2);
        const std::int32_t back = -static_cast<std::int32_t>(body_halfwords + 2);
        push16(p.code, static_cast<std::uint16_t>(0xD100u | (back & 0xFF))); // bne body
        push16(p.code, 0xE7FE); // b .

        p.budget = iterations * 16 + 256;
        if (!run_pair("loops", dc, da, p, seed, true, cov)) failures++;
    }
    return failures;
}

// Regression test for the VMLA/VMLS product-rounding fix: s8 = s9 * s10
// followed by s8 -= s9 * s10 must be exactly zero, because a chained (non-fused)
// multiply-accumulate rounds the product to the destination precision before
// accumulating it. dyncom used to carry the multiply's extra significand bits
// into the add and leave a sub-ulp residue.
std::uint32_t check_vfp_mac_cancellation(backend_env &dc, backend_env &da) {
    program p;
    p.addr = PROG_CODE_LO;
    p.kind = "vfp-mac-cancellation";
    p.vfp_init[9] = 0x3F800001u;  // 1.0000001f
    p.vfp_init[10] = 0x40533333u; // 3.3f
    push32(p.code, 0xEE244A85u);  // vmul.f32 s8, s9, s10
    push32(p.code, 0xEE044AC5u);  // vmls.f32 s8, s9, s10
    push32(p.code, 0xEAFFFFFEu);
    p.budget = 64;

    install(dc, p);
    install(da, p);
    dc.cpu->clear_instruction_cache();
    da.cpu->clear_instruction_cache();
    dc.cpu->imb_range(p.addr, p.code.size());
    da.cpu->imb_range(p.addr, p.code.size());

    const full_state sd = execute(dc, p);
    const full_state sa = execute(da, p);
    if (sd.vfp[8] == sa.vfp[8] && sd.vfp[8] == 0) {
        return 0;
    }
    std::printf("[DIVERGENCE] vfp-mac-cancellation: vmls after vmul leaves dyncom "
                "s8=%08X, dynarmic s8=%08X; expected exactly zero (the product must "
                "be rounded to single before it is accumulated)\n",
        sd.vfp[8], sa.vfp[8]);
    return 1;
}

// VFP: the host-float fast path against dynarmic's own VFP implementation.
std::uint32_t suite_vfp(backend_env &dc, backend_env &da, std::uint32_t base_seed,
    std::uint32_t count, coverage &cov) {
    // vadd/vsub/vmul/vdiv/vmla/vmls .f32 over s8..s15, then store the bank.
    static constexpr std::uint32_t vfp_ops[] = {
        0xEE344A05u, // vadd.f32 s8, s8, s10
        0xEE344A45u, // vsub.f32 s8, s8, s10
        0xEE244A85u, // vmul.f32 s8, s9, s10
        0xEE844A85u, // vdiv.f32 s8, s9, s10
        0xEE044A85u, // vmla.f32 s8, s9, s10
        0xEE044AC5u, // vmls.f32 s8, s9, s10
        0xEEB04AC5u, // vabs.f32 s8, s10
        0xEEB14A45u, // vneg.f32 s8, s10
    };

    std::uint32_t failures = 0;
    for (std::uint32_t i = 0; i < count && failures < 10; i++) {
        const std::uint32_t seed = base_seed + i;
        rng r(seed);

        program p;
        p.addr = PROG_CODE_LO;
        p.thumb = false;
        p.kind = "vfp";
        set_common_init(p, r);
        for (int v = 8; v < 16; v++) p.vfp_init[v] = random_normal_f32(r);

        const std::uint32_t len = 4 + r.range(12);
        for (std::uint32_t n = 0; n < len; n++) {
            push32(p.code, vfp_ops[r.range(sizeof(vfp_ops) / sizeof(vfp_ops[0]))]);
        }
        // Spill the bank so a divergence also shows up in memory.
        push32(p.code, 0xED894A00u); // vstr s8, [r9]
        push32(p.code, 0xEDC94A01u); // vstr s9, [r9, #4]
        push32(p.code, 0xEAFFFFFEu);

        p.budget = len * 4 + 64;
        if (!run_pair("vfp", dc, da, p, seed, true, cov)) failures++;
    }
    return failures;
}

} // namespace

int main(int argc, char **argv) {
    std::uint32_t base_seed = 1;
    std::uint32_t count = 200000;
    if (argc > 1) base_seed = static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 0));
    if (argc > 2) count = static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 0));

    std::printf("dyncom_difftest: phase 1 (golden model, single instruction): "
                "data-processing + load/store + ldm/stm + halfword, %u cases from seed %u\n",
        count, base_seed);

    diff_env env_a, env_b;
    auto core_a = make_core(env_a);
    auto core_b = make_core(env_b);
    std::vector<std::uint8_t> golden_mem(MEM_SIZE, 0);

    std::uint32_t failures = 0;
    auto window_eq = [](const std::vector<std::uint8_t> &x, const std::vector<std::uint8_t> &y) {
        return std::memcmp(x.data() + LS_DATA_LO, y.data() + LS_DATA_LO, LS_DATA_HI - LS_DATA_LO) == 0;
    };

    for (std::uint32_t i = 0; i < count; i++) {
        const std::uint32_t seed = base_seed + i;
        rng r(seed);

        cpu_state init = gen_state(r);
        const std::uint32_t kind = r.range(5); // 0,1 data-proc; 2 single; 3 ldm/stm; 4 halfword
        const bool touches_mem = (kind >= 2);
        std::uint32_t inst;
        const char *kind_name;
        if (kind < 2) {
            inst = gen_data_processing(r);
            kind_name = "dyncom != golden";
        } else if (kind == 2) {
            inst = gen_load_store(r, init);
            kind_name = "dyncom != golden (load/store)";
        } else if (kind == 3) {
            inst = gen_ldm_stm(r, init);
            kind_name = "dyncom != golden (ldm/stm)";
        } else {
            inst = gen_halfword(r, init);
            kind_name = "dyncom != golden (halfword)";
        }

        // Place the instruction at PC 0 in both envs and seed the data window
        // identically in A, B and the golden image (memory-touching cases only).
        std::memcpy(env_a.mem.data(), &inst, 4);
        std::memcpy(env_b.mem.data(), &inst, 4);
        if (touches_mem) {
            for (std::uint32_t a = LS_DATA_LO; a < LS_DATA_HI; a += 4) {
                const std::uint32_t w = r.u32();
                std::memcpy(env_a.mem.data() + a, &w, 4);
                std::memcpy(env_b.mem.data() + a, &w, 4);
                std::memcpy(golden_mem.data() + a, &w, 4);
            }
        }

        core_a->imb_range(0, 8);
        core_b->imb_range(0, 8);

        write_state(*core_a, init);
        write_state(*core_b, init);

        core_a->set_pc(0);
        core_b->set_pc(0);
        core_a->run(1);
        core_b->run(1);

        const cpu_state got_a = read_state(*core_a);
        const cpu_state got_b = read_state(*core_b);
        cpu_state golden;
        if (kind < 2) {
            golden = golden_data_processing(inst, init);
        } else if (kind == 2) {
            golden = golden_load_store(inst, init, golden_mem.data());
        } else if (kind == 3) {
            golden = golden_ldm_stm(inst, init, golden_mem.data());
        } else {
            golden = golden_halfword(inst, init, golden_mem.data());
        }

        // (a) dyncom vs the independent golden model (registers + memory).
        if (!(got_a == golden) || (touches_mem && !window_eq(env_a.mem, golden_mem))) {
            report_mismatch(kind_name, inst, seed, golden, got_a);
            if (++failures >= 20) break;
            continue;
        }
        // (b) self-A/B (determinism today; a flag-gated optimization later).
        if (!(got_a == got_b) || (touches_mem && !window_eq(env_a.mem, env_b.mem))) {
            report_mismatch("core A != core B", inst, seed, got_a, got_b);
            if (++failures >= 20) break;
        }
    }

    failures += run_vfp_mac_differential(env_a, env_b, *core_a, *core_b,
        base_seed, count);
    failures += benchmark_vfp_mac(env_a, env_b, *core_a, *core_b);

    // Negative control: prove the comparator catches a deliberate divergence.
    {
        cpu_state x{}, y{};
        y.reg[3] = 1;
        if (x == y) {
            std::printf("[HARNESS BUG] comparator failed to detect an injected divergence\n");
            failures++;
        }
    }

    // -----------------------------------------------------------------------
    // Phase 2: whole programs, dyncom vs dynarmic.
    // -----------------------------------------------------------------------
    std::printf("dyncom_difftest: phase 2 (dynarmic oracle, whole programs)\n");

    auto dc = make_dyncom_env();
    auto da = make_dynarmic_env();
    coverage cov;

    const std::uint32_t programs = (count / 200 < 4) ? 4 : (count / 200);
    const std::uint32_t rounds = (programs / 8 < 2) ? 2 : (programs / 8);

    struct suite_result {
        const char *name;
        std::uint32_t failures;
    };
    const suite_result results[] = {
        { "cold  ", suite_cold(*dc, *da, base_seed, programs, cov) },
        { "arena ", suite_arena(*dc, *da, base_seed, rounds, cov) },
        { "asid  ", suite_asid(*dc, *da, base_seed, rounds * 4, cov) },
        { "loops ", suite_loops(*dc, *da, base_seed, programs / 2 + 4, cov) },
        { "vfp   ", suite_vfp(*dc, *da, base_seed, programs / 2 + 4, cov) },
    };
    failures += check_vfp_mac_cancellation(*dc, *da);
    for (const suite_result &sr : results) {
        std::printf("  %s %s\n", sr.name, sr.failures ? "FAIL" : "ok");
        failures += sr.failures;
    }

    std::printf("  coverage: %llu programs, %llu dynarmic interpreter-fallback insts, "
                "%llu loop-accelerator attaches (%llu bulk iterations)\n",
        static_cast<unsigned long long>(cov.programs),
        static_cast<unsigned long long>(cov.fallback_insts),
        static_cast<unsigned long long>(cov.accel_attaches),
        static_cast<unsigned long long>(cov.accel_bulk_iterations));

    // Coverage assertions. Without these a suite that silently stopped
    // reaching its target would report a clean pass -- which is exactly how
    // the previous harness "verified" the loop accelerator it never ran.
    if (cov.accel_attaches == 0) {
        std::printf("[HARNESS BUG] the loop accelerator never attached; the loop "
                    "suite is not testing it\n");
        failures++;
    }
    if (cov.fallback_insts != 0) {
        std::printf("[HARNESS BUG] dynarmic used its embedded interpreter for %llu "
                    "instruction(s); those cases were not independent\n",
            static_cast<unsigned long long>(cov.fallback_insts));
        failures++;
    }

    if (failures == 0) {
        std::printf("dyncom_difftest: PASS (%u single-instruction cases vs golden, "
                    "%llu programs vs dynarmic, VFP soft/host A/B, negative control)\n",
            count, static_cast<unsigned long long>(cov.programs));
        return 0;
    }
    std::printf("dyncom_difftest: FAIL (%u divergences)\n", failures);
    return 1;
}
