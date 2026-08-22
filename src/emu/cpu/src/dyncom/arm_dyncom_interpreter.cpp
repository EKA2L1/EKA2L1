// Copyright 2012 Michael Kang, 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#define CITRA_IGNORE_EXIT(x)

#include <algorithm>
#include <bit>
#include <cinttypes>
#include <common/log.h>
#include <common/types.h>
#include <cpu/dyncom/arm_dyncom_dec.h>
#include <cpu/dyncom/arm_dyncom_interpreter.h>
#include <cpu/dyncom/arm_dyncom_run.h>
#include <cpu/dyncom/arm_dyncom_thumb.h>
#include <cpu/dyncom/arm_dyncom_trans.h>
#include <cpu/dyncom/armstate.h>
#include <cpu/dyncom/armsupp.h>
#include <cpu/dyncom/vfp/vfp.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <cpu/arm_interface.h>

#define RM BITS(sht_oper, 0, 3)
#define RS BITS(sht_oper, 8, 11)

#define glue(x, y) x##y
#define DPO(s) glue(DataProcessingOperands, s)
#define ROTATE_RIGHT(n, i, l) ((n << (l - i)) | (n >> i))
#define ROTATE_LEFT(n, i, l) ((n >> (l - i)) | (n << i))
#define ROTATE_RIGHT_32(n, i) ROTATE_RIGHT(n, i, 32)
#define ROTATE_LEFT_32(n, i) ROTATE_LEFT(n, i, 32)

// Reference predicate for one (cond, NZCV) combination; only used at compile
// time to build the branchless lookup table below.
static constexpr bool CondPassedRef(unsigned int cond, bool n_flag, bool z_flag, bool c_flag,
    bool v_flag) {
    switch (cond) {
    case ConditionCode::EQ:
        return z_flag;
    case ConditionCode::NE:
        return !z_flag;
    case ConditionCode::CS:
        return c_flag;
    case ConditionCode::CC:
        return !c_flag;
    case ConditionCode::MI:
        return n_flag;
    case ConditionCode::PL:
        return !n_flag;
    case ConditionCode::VS:
        return v_flag;
    case ConditionCode::VC:
        return !v_flag;
    case ConditionCode::HI:
        return (c_flag && !z_flag);
    case ConditionCode::LS:
        return (!c_flag || z_flag);
    case ConditionCode::GE:
        return (n_flag == v_flag);
    case ConditionCode::LT:
        return (n_flag != v_flag);
    case ConditionCode::GT:
        return (!z_flag && (n_flag == v_flag));
    case ConditionCode::LE:
        return (z_flag || (n_flag != v_flag));
    case ConditionCode::AL:
    case ConditionCode::NV: // Unconditional
        return true;
    }

    return false;
}

// Bit f of kCondPassedTable[cond] answers CondPassedRef for the flag nibble
// f = N<<3 | Z<<2 | C<<1 | V, turning the per-instruction predicate into one
// load + shift that the compiler can inline at every handler site.
static constexpr std::uint16_t BuildCondMask(unsigned int cond) {
    std::uint16_t mask = 0;
    for (unsigned int f = 0; f < 16; ++f) {
        if (CondPassedRef(cond, (f >> 3) & 1, (f >> 2) & 1, (f >> 1) & 1, f & 1))
            mask = static_cast<std::uint16_t>(mask | (1u << f));
    }
    return mask;
}

static constexpr std::uint16_t kCondPassedTable[16] = {
    BuildCondMask(0), BuildCondMask(1), BuildCondMask(2), BuildCondMask(3),
    BuildCondMask(4), BuildCondMask(5), BuildCondMask(6), BuildCondMask(7),
    BuildCondMask(8), BuildCondMask(9), BuildCondMask(10), BuildCondMask(11),
    BuildCondMask(12), BuildCondMask(13), BuildCondMask(14), BuildCondMask(15)
};

#if defined(_MSC_VER)
#define DYNCOM_FORCE_INLINE __forceinline
#else
#define DYNCOM_FORCE_INLINE inline __attribute__((always_inline))
#endif

static DYNCOM_FORCE_INLINE bool CondPassed(const ARMul_State *cpu, unsigned int cond) {
    const unsigned int flags = ((cpu->NFlag != 0) << 3) | ((cpu->ZFlag != 0) << 2)
        | ((cpu->CFlag != 0) << 1) | (cpu->VFlag != 0);
    return (kCondPassedTable[cond & 15] >> flags) & 1;
}

// ---------------------------------------------------------------------------
// Optional guest-execution profiler (-DEKA2L1_DYNCOM_PROFILE). Counts, per
// executed instruction, the opcode and the (previous,current) consecutive pair
// within a basic block, plus the per-block instruction-count distribution, and
// periodically logs the hottest opcodes / pairs. Used to pick which instruction
// patterns are worth fusing into super-ops. Zero overhead when not defined.
// ---------------------------------------------------------------------------
#ifdef EKA2L1_DYNCOM_PROFILE
#include <algorithm>
#include <vector>
namespace {
    constexpr int PROF_NUM_OPS = 202; // == arm_instruction_trans_len
    const char *kProfOpNames[PROF_NUM_OPS] = {
        "VMLA","VMLS","VNMLA","VNMLS","VNMUL","VMUL","VADD","VSUB","VDIV","VMOVI","VMOVR","VABS","VNEG","VSQRT","VCMP",
        "VCMP2","VCVTBDS","VCVTBFF","VCVTBFI","VMOVBRS","VMSR","VMOVBRC","VMRS","VMOVBCR","VMOVBRRSS","VMOVBRRD","VSTR",
        "VPUSH","VSTM","VPOP","VLDR","VLDM","SRS","RFE","BKPT","BLX","CPS","PLD","SETEND","CLREX","REV16","USAD8","SXTB",
        "UXTB","SXTH","SXTB16","UXTH","UXTB16","CPY","UXTAB","SSUB8","SHSUB8","SSUBADDX","STREX","STREXB","SWP","SWPB",
        "SSUB16","SSAT16","SHSUBADDX","QSUBADDX","SHADDSUBX","SHADD8","SHADD16","SEL","SADDSUBX","SADD8","SADD16","SHSUB16",
        "UMAAL","UXTAB16","USUBADDX","USUB8","USUB16","USAT16","USADA8","UQSUBADDX","UQSUB8","UQSUB16","UQADDSUBX","UQADD8",
        "UQADD16","SXTAB","UHSUBADDX","UHSUB8","UHSUB16","UHADDSUBX","UHADD8","UHADD16","UADDSUBX","UADD8","UADD16","SXTAH",
        "SXTAB16","QADD8","BXJ","CLZ","UXTAH","BX","REV","BLX2","REVSH","QADD","QADD16","QADDSUBX","LDREX","QDADD","QDSUB",
        "QSUB","LDREXB","QSUB8","QSUB16","SMUAD","SMMUL","SMUSD","SMLSD","SMLSLD","SMMLA","SMMLS","SMLALD","SMLAD","SMLAW",
        "SMULW","PKHTB","PKHBT","SMUL","SMLALXY","SMLA","MCRR","MRRC","CMP","TST","TEQ","CMN","SMULL","UMULL","UMLAL",
        "SMLAL","MUL","MLA","SSAT","USAT","MRS","MSR","AND","BIC","LDM","EOR","ADD","RSB","RSC","SBC","ADC","SUB","ORR",
        "MVN","MOV","STM","LDM2","LDRSH","STM2","LDM3","LDRSB","STRD","LDRH","STRH","LDRD","STRT","STRBT","LDRBT","LDRT",
        "MRC","MCR","MSR2","MSR3","MSR4","MSR5","MSR6","LDRB","STRB","LDR","LDRCOND","STR","CDP","STC","LDC","LDREXD",
        "STREXD","LDREXH","STREXH","NOP","YIELD","WFE","WFI","SEV","SWI","BBL","B_2_THUMB","B_COND_THUMB","BL_1_THUMB",
        "BL_2_THUMB","BLX_1_THUMB"
    };
    inline const char *prof_op_name(int idx) {
        return (idx >= 0 && idx < PROF_NUM_OPS) ? kProfOpNames[idx] : "?";
    }
    struct dyncom_profiler {
        std::uint64_t op_count[PROF_NUM_OPS] = {};
        std::vector<std::uint64_t> pair_count;
        std::uint64_t block_len[64] = {};
        std::uint64_t total_insts = 0;
        std::uint64_t total_blocks = 0;
        std::uint64_t next_dump = 50000000ull;
        // Sparse guest-PC histogram (64-byte buckets, sampled every 64th
        // instruction) to attribute hot loops to guest code regions. The code
        // bytes are snapshotted on first touch, while the bucket's page is
        // guaranteed mapped in the current process (reading them at dump time
        // from an unrelated process faults the guest).
        struct pc_bucket {
            std::uint64_t count = 0;
            std::uint32_t code[16] = {};
        };
        std::unordered_map<std::uint32_t, pc_bucket> pc_hist;
        std::uint64_t pc_samples = 0;
        dyncom_profiler()
            : pair_count(static_cast<std::size_t>(PROF_NUM_OPS) * PROF_NUM_OPS, 0) {}
    };
    dyncom_profiler g_dyncom_profiler;

    void dyncom_profile_dump(ARMul_State *cpu) {
        dyncom_profiler &p = g_dyncom_profiler;
        if (p.total_insts == 0)
            return;
        // Write to a plain file in the cwd (Documents/data on iOS) so the output
        // bypasses the per-category log filter (CPU.DynCom is silenced there).
        std::FILE *f = std::fopen("dyncom_profile.txt", "w");
        if (!f)
            return;
        std::fprintf(f, "=== dyncom profile: %llu insts, %llu blocks, %.2f insts/block ===\n",
            (unsigned long long)p.total_insts, (unsigned long long)p.total_blocks,
            p.total_blocks ? (double)p.total_insts / p.total_blocks : 0.0);
        std::vector<int> ops(PROF_NUM_OPS);
        for (int i = 0; i < PROF_NUM_OPS; i++) ops[i] = i;
        std::sort(ops.begin(), ops.end(), [&](int a, int b) { return p.op_count[a] > p.op_count[b]; });
        for (int i = 0; i < 25 && p.op_count[ops[i]]; i++)
            std::fprintf(f, "  op   %-12s %13llu (%.1f%%)\n", prof_op_name(ops[i]), (unsigned long long)p.op_count[ops[i]],
                100.0 * (double)p.op_count[ops[i]] / p.total_insts);
        std::vector<std::size_t> pairs;
        for (std::size_t i = 0; i < p.pair_count.size(); i++)
            if (p.pair_count[i]) pairs.push_back(i);
        std::sort(pairs.begin(), pairs.end(), [&](std::size_t a, std::size_t b) { return p.pair_count[a] > p.pair_count[b]; });
        for (std::size_t i = 0; i < 40 && i < pairs.size(); i++) {
            const int a = static_cast<int>(pairs[i] / PROF_NUM_OPS);
            const int b = static_cast<int>(pairs[i] % PROF_NUM_OPS);
            std::fprintf(f, "  pair %-10s -> %-10s %13llu (%.1f%%)\n", prof_op_name(a), prof_op_name(b),
                (unsigned long long)p.pair_count[pairs[i]], 100.0 * (double)p.pair_count[pairs[i]] / p.total_insts);
        }
        for (int i = 0; i < 16; i++)
            if (p.block_len[i])
                std::fprintf(f, "  blocklen %2d : %13llu\n", i, (unsigned long long)p.block_len[i]);
        if (p.pc_samples) {
            std::vector<std::pair<std::uint32_t, const dyncom_profiler::pc_bucket *>> hot;
            hot.reserve(p.pc_hist.size());
            for (const auto &kv : p.pc_hist)
                hot.emplace_back(kv.first, &kv.second);
            std::sort(hot.begin(), hot.end(),
                [](const auto &a, const auto &b) { return a.second->count > b.second->count; });
            std::fprintf(f, "  pc-hist: %llu samples, %zu buckets (64B)\n",
                (unsigned long long)p.pc_samples, p.pc_hist.size());
            for (std::size_t i = 0; i < 48 && i < hot.size(); i++)
                std::fprintf(f, "  pc %08X %13llu (%.2f%%)\n", hot[i].first,
                    (unsigned long long)hot[i].second->count,
                    100.0 * (double)hot[i].second->count / p.pc_samples);
            for (std::size_t i = 0; i < 10 && i < hot.size(); i++) {
                std::fprintf(f, "  code %08X:", hot[i].first);
                for (int w = 0; w < 16; w++)
                    std::fprintf(f, " %08X", hot[i].second->code[w]);
                std::fprintf(f, "\n");
            }
        }
        std::fclose(f);
    }
}
#define PROF_STEP(cpu, the_idx)                                                              \
    do {                                                                                     \
        dyncom_profiler &pp_ = g_dyncom_profiler;                                            \
        const int idx_ = (the_idx);                                                          \
        if (idx_ >= PROF_NUM_OPS)                                                             \
            break; /* synthetic ops (loop accel) have no table slot */                        \
        pp_.op_count[idx_]++;                                                                 \
        pp_.total_insts++;                                                                    \
        if ((pp_.total_insts & 63) == 0) {                                                    \
            const std::uint32_t pcb_ = (cpu)->Reg[15] & ~63u;                                 \
            auto &bkt_ = pp_.pc_hist[pcb_];                                                   \
            if (bkt_.count++ == 0)                                                            \
                for (int w_ = 0; w_ < 16; w_++)                                               \
                    bkt_.code[w_] = (cpu)->ReadMemory32(pcb_ + w_ * 4);                       \
            pp_.pc_samples++;                                                                 \
        }                                                                                     \
        if ((cpu)->prof_prev >= 0)                                                            \
            pp_.pair_count[static_cast<std::size_t>((cpu)->prof_prev) * PROF_NUM_OPS + idx_]++; \
        (cpu)->prof_prev = idx_;                                                              \
        if ((cpu)->prof_block_len < 63)                                                       \
            (cpu)->prof_block_len++;                                                          \
        if (pp_.total_insts >= pp_.next_dump) {                                               \
            dyncom_profile_dump(cpu);                                                         \
            pp_.next_dump += 50000000ull;                                                    \
        }                                                                                     \
    } while (0)
#define PROF_BLOCK_ENTER(cpu)                                                                \
    do {                                                                                     \
        dyncom_profiler &pp_ = g_dyncom_profiler;                                            \
        pp_.total_blocks++;                                                                   \
        pp_.block_len[(cpu)->prof_block_len & 63]++;                                          \
        (cpu)->prof_block_len = 0;                                                            \
        (cpu)->prof_prev = -1;                                                                \
    } while (0)
#else
#define PROF_STEP(cpu, the_idx) ((void)0)
#define PROF_BLOCK_ENTER(cpu) ((void)0)
#endif

// ---------------------------------------------------------------------------
// Translation-time bulk-loop acceleration.
//
// Guest-side pixel-conversion / copy / fill loops (e.g. an RGB565->32bpp
// convert measured at 14% of Brothers in Arms gameplay) interpret a handful
// of instructions per element. When a translated Thumb block is a
// single-block do-while loop of the canonical shape
//
//     do { v = load(src); store(dst, f(v)); src += s; dst += d; }
//     while (--counter != 0);
//
// a symbolic one-iteration simulation proves the shape and a synthetic
// instruction prepended to the block then executes up to counter-1
// iterations natively through the dyncom TLB. The LAST iteration is always
// left to the interpreter, so flags, scratch registers and the loop exit
// come out of real interpretation; the bulk step only advances the
// induction registers (base pointers + counter), whose per-iteration deltas
// the matcher proved constant. Any TLB miss simply stops the bulk step and
// hands the remainder back to the interpreter, so faults and IPC unmapping
// keep their interpreted behaviour.
// ---------------------------------------------------------------------------

// Dispatch index of the synthetic bulk-loop instruction. The label table ends
// with DISPATCH/INIT_INST_LENGTH/END at 202..204 (kept for the switch
// fallback); 205 is the first free slot and is only ever produced by the
// emitter below, never by the decoder.
static constexpr unsigned int LOOP_ACCEL_IDX = 205;

enum accel_chain_op : std::uint8_t {
    ACC_SHL,
    ACC_SHR,
    ACC_SAR,
    ACC_AND,
    ACC_ORR,
    ACC_EOR,
    ACC_ADD,
    ACC_SUB,
    ACC_SXTB,
    ACC_SXTH,
};

enum accel_val_kind : std::uint8_t {
    ACC_VAL_CONST, // constant
    ACC_VAL_CHAIN, // ops applied to the iteration's loaded value
    ACC_VAL_REG, // invariant register + offset
};

struct accel_store {
    std::int32_t off; // relative to the dst base register's iteration value
    std::uint8_t width; // 1/2/4 bytes
    std::uint8_t kind; // accel_val_kind
    std::uint8_t reg; // ACC_VAL_REG: the invariant register
    std::uint8_t nops; // ACC_VAL_CHAIN: number of chain ops
    std::uint32_t cval; // ACC_VAL_CONST value / ACC_VAL_REG offset
    struct {
        std::uint8_t op;
        std::uint32_t imm;
    } ops[8];
};

struct loop_accel_inst {
    std::uint8_t has_load;
    std::uint8_t load_width; // 1/2/4
    std::uint8_t load_signed;
    std::uint8_t src_reg;
    std::int32_t src_off;
    std::int32_t src_step; // > 0

    std::uint8_t dst_reg;
    std::int32_t dst_min_off;
    std::int32_t dst_step; // >= span, > 0
    std::uint8_t span; // dense bytes written per iteration
    std::uint8_t store_count;
    accel_store stores[4];

    std::uint8_t ind_count;
    struct {
        std::uint8_t reg;
        std::int32_t delta;
    } ind[8];

    std::uint8_t counter_reg;
    std::uint16_t body_len; // guest instructions per iteration (incl. branch)
};

namespace {
    // Symbolic value for the one-iteration abstract interpretation.
    struct accel_sym {
        enum kind_t : std::uint8_t { S_CONST,
            S_AFFINE, // INIT(reg) + c
            S_CHAIN, // chain ops over the loaded value
            S_TOP } kind;
        std::uint8_t reg; // S_AFFINE base
        std::uint8_t nops;
        std::uint32_t c; // S_CONST value / S_AFFINE offset
        struct {
            std::uint8_t op;
            std::uint32_t imm;
        } ops[8];

        static accel_sym make_const(std::uint32_t v) {
            accel_sym s{};
            s.kind = S_CONST;
            s.c = v;
            return s;
        }
        static accel_sym make_affine(std::uint8_t r, std::uint32_t off) {
            accel_sym s{};
            s.kind = S_AFFINE;
            s.reg = r;
            s.c = off;
            return s;
        }
        static accel_sym make_top() {
            accel_sym s{};
            s.kind = S_TOP;
            return s;
        }
        bool push_op(std::uint8_t op, std::uint32_t imm) {
            if (nops >= 8)
                return false;
            ops[nops].op = op;
            ops[nops].imm = imm;
            nops++;
            return true;
        }
    };

    inline std::uint32_t accel_apply_op(std::uint32_t v, std::uint8_t op, std::uint32_t imm) {
        switch (op) {
        case ACC_SHL:
            return (imm >= 32) ? 0 : (v << imm);
        case ACC_SHR:
            return (imm >= 32) ? 0 : (v >> imm);
        case ACC_SAR:
            return static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> ((imm >= 32) ? 31 : imm));
        case ACC_AND:
            return v & imm;
        case ACC_ORR:
            return v | imm;
        case ACC_EOR:
            return v ^ imm;
        case ACC_ADD:
            return v + imm;
        case ACC_SUB:
            return v - imm;
        case ACC_SXTB:
            return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(v)));
        case ACC_SXTH:
            return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(v)));
        }
        return v;
    }
}

#if defined(EKA2L1_DYNCOM_DIFFTEST)
// Test-only instrumentation. The accelerator only ever attaches to block
// translation, so a harness that never reaches it would score "no divergence"
// while proving nothing about it; these let the harness assert it was exercised.
static std::uint64_t g_loop_accel_attaches = 0;
static std::uint64_t g_loop_accel_bulk_iterations = 0;

namespace eka2l1::arm {
    void dyncom_reset_loop_accel_counters_for_test() {
        g_loop_accel_attaches = 0;
        g_loop_accel_bulk_iterations = 0;
    }

    std::uint64_t dyncom_loop_accel_attaches_for_test() {
        return g_loop_accel_attaches;
    }

    std::uint64_t dyncom_loop_accel_bulk_iterations_for_test() {
        return g_loop_accel_bulk_iterations;
    }
}
#endif

// Symbolically simulate one iteration of the candidate Thumb loop at
// [pc_start .. terminal Bcc]. Returns true and fills `out` when the body is a
// provably uniform single-load copy/convert/fill loop.
static bool analyze_thumb_bulk_loop(ARMul_State *cpu, std::uint32_t pc_start, loop_accel_inst &out) {
    constexpr int MAX_BODY = 24;

    accel_sym sym[8];
    for (int i = 0; i < 8; i++)
        sym[i] = accel_sym::make_affine(static_cast<std::uint8_t>(i), 0);

    enum { ACC_NONE,
        ACC_READ,
        ACC_WRITE };
    std::uint8_t first_access[8] = {};

    enum { FLAG_OTHER,
        FLAG_CMP0,
        FLAG_SUB1 };
    int last_flag = FLAG_OTHER;
    int flag_reg = -1;

    bool have_load = false;
    bool store_seen = false; // bulk exec is load-then-stores: reject other orders
    std::uint8_t load_width = 0, load_signed = 0, src_reg = 0;
    std::int32_t src_off = 0;

    int store_count = 0;
    struct {
        std::uint8_t base;
        std::int32_t off;
        std::uint8_t width;
        accel_sym val;
    } raw_stores[4];

    // Reading a register: record read-before-write and return its symbol.
    auto rd_sym = [&](int r) -> accel_sym & {
        if (first_access[r] == ACC_NONE)
            first_access[r] = ACC_READ;
        return sym[r];
    };
    auto wr_sym = [&](int r, const accel_sym &v) {
        if (first_access[r] == ACC_NONE)
            first_access[r] = ACC_WRITE;
        sym[r] = v;
    };

    // rd = rn OP imm-const.
    auto apply_const = [&](const accel_sym &a, std::uint8_t op, std::uint32_t imm) -> accel_sym {
        accel_sym r = a;
        switch (a.kind) {
        case accel_sym::S_CONST:
            r.c = accel_apply_op(a.c, op, imm);
            return r;
        case accel_sym::S_AFFINE:
            if (op == ACC_ADD) {
                r.c = a.c + imm;
                return r;
            }
            if (op == ACC_SUB) {
                r.c = a.c - imm;
                return r;
            }
            return accel_sym::make_top();
        case accel_sym::S_CHAIN:
            if (!r.push_op(op, imm))
                return accel_sym::make_top();
            return r;
        default:
            return accel_sym::make_top();
        }
    };

    std::uint32_t pc = pc_start;
    int count = 0;
    bool matched_branch = false;

    while (count < MAX_BODY) {
        const std::uint32_t word = cpu->ReadCode(pc & 0xFFFFFFFC);
        const std::uint16_t hw = (pc & 2) ? static_cast<std::uint16_t>(word >> 16)
                                          : static_cast<std::uint16_t>(word & 0xFFFF);
        count++;

        if ((hw & 0xF000) == 0xD000) {
            // Bcc / SWI. Terminal only, must be BNE back to pc_start.
            const std::uint32_t cond = (hw >> 8) & 0xF;
            if (cond >= 0xE)
                return false; // undefined / SWI
            const std::int32_t soff = static_cast<std::int32_t>(static_cast<std::int8_t>(hw & 0xFF)) * 2;
            const std::uint32_t target = pc + 4 + soff;
            if (cond != 0x1 /*NE*/ || target != pc_start)
                return false;
            matched_branch = true;
            break;
        }

        switch (hw >> 13) {
        case 0: { // 000: shift imm / add-sub reg-imm3
            if (((hw >> 11) & 3) != 3) {
                const int opk = (hw >> 11) & 3; // LSL/LSR/ASR
                const int rm = (hw >> 3) & 7, rd = hw & 7;
                std::uint32_t imm = (hw >> 6) & 31;
                std::uint8_t op = (opk == 0) ? ACC_SHL : (opk == 1) ? ACC_SHR
                                                                    : ACC_SAR;
                if (opk != 0 && imm == 0)
                    imm = 32; // LSR/ASR #32 encodings
                wr_sym(rd, apply_const(rd_sym(rm), op, imm));
                last_flag = FLAG_OTHER;
            } else {
                const int rd = hw & 7, rn = (hw >> 3) & 7;
                const bool sub = (hw >> 9) & 1;
                const bool immf = (hw >> 10) & 1;
                const int rm_or_imm = (hw >> 6) & 7;
                accel_sym res;
                const accel_sym a = rd_sym(rn);
                if (immf) {
                    res = apply_const(a, sub ? ACC_SUB : ACC_ADD, static_cast<std::uint32_t>(rm_or_imm));
                    if (sub && rm_or_imm == 1 && rd == rn) {
                        last_flag = FLAG_SUB1;
                        flag_reg = rd;
                    } else {
                        last_flag = FLAG_OTHER;
                    }
                } else {
                    const accel_sym b = rd_sym(rm_or_imm);
                    if (b.kind == accel_sym::S_CONST)
                        res = apply_const(a, sub ? ACC_SUB : ACC_ADD, b.c);
                    else if (!sub && a.kind == accel_sym::S_CONST)
                        res = apply_const(b, ACC_ADD, a.c);
                    else
                        res = accel_sym::make_top();
                    last_flag = FLAG_OTHER;
                }
                wr_sym(rd, res);
            }
            break;
        }
        case 1: { // 001: MOV/CMP/ADD/SUB imm8
            const int opk = (hw >> 11) & 3;
            const int rd = (hw >> 8) & 7;
            const std::uint32_t imm = hw & 0xFF;
            switch (opk) {
            case 0: // MOV
                wr_sym(rd, accel_sym::make_const(imm));
                last_flag = FLAG_OTHER;
                break;
            case 1: { // CMP
                const accel_sym &a = rd_sym(rd);
                if (imm == 0 && a.kind == accel_sym::S_AFFINE && a.reg == rd) {
                    last_flag = FLAG_CMP0;
                    flag_reg = rd;
                } else {
                    last_flag = FLAG_OTHER;
                }
                break;
            }
            case 2: // ADD
                wr_sym(rd, apply_const(rd_sym(rd), ACC_ADD, imm));
                last_flag = FLAG_OTHER;
                break;
            case 3: // SUB
                wr_sym(rd, apply_const(rd_sym(rd), ACC_SUB, imm));
                if (imm == 1) {
                    last_flag = FLAG_SUB1;
                    flag_reg = rd;
                } else {
                    last_flag = FLAG_OTHER;
                }
                break;
            }
            break;
        }
        case 2: { // 010: ALU reg / hi-reg / PC-literal / reg-offset mem
            if ((hw & 0xFC00) == 0x4000) { // format 4 ALU
                const int aop = (hw >> 6) & 0xF;
                const int rm = (hw >> 3) & 7, rd = hw & 7;
                switch (aop) {
                case 0x0: // AND
                case 0x1: // EOR
                case 0xC: { // ORR
                    const accel_sym b = rd_sym(rm);
                    const accel_sym a = rd_sym(rd);
                    const std::uint8_t op = (aop == 0x0) ? ACC_AND : (aop == 0x1) ? ACC_EOR
                                                                                  : ACC_ORR;
                    if (b.kind == accel_sym::S_CONST)
                        wr_sym(rd, apply_const(a, op, b.c));
                    else if (a.kind == accel_sym::S_CONST)
                        wr_sym(rd, apply_const(b, op, a.c));
                    else
                        wr_sym(rd, accel_sym::make_top());
                    last_flag = FLAG_OTHER;
                    break;
                }
                case 0x2: // LSL reg
                case 0x3: // LSR reg
                case 0x4: { // ASR reg
                    const accel_sym b = rd_sym(rm);
                    const accel_sym a = rd_sym(rd);
                    const std::uint8_t op = (aop == 0x2) ? ACC_SHL : (aop == 0x3) ? ACC_SHR
                                                                                  : ACC_SAR;
                    if (b.kind == accel_sym::S_CONST && (b.c & 0xFF) < 32)
                        wr_sym(rd, apply_const(a, op, b.c & 0xFF));
                    else
                        wr_sym(rd, accel_sym::make_top());
                    last_flag = FLAG_OTHER;
                    break;
                }
                case 0x8: // TST
                case 0xA: // CMP reg
                case 0xB: // CMN
                    rd_sym(rm);
                    rd_sym(rd);
                    last_flag = FLAG_OTHER;
                    break;
                case 0x9: { // NEG (RSB #0)
                    const accel_sym b = rd_sym(rm);
                    if (b.kind == accel_sym::S_CONST)
                        wr_sym(rd, accel_sym::make_const(0u - b.c));
                    else
                        wr_sym(rd, accel_sym::make_top());
                    last_flag = FLAG_OTHER;
                    break;
                }
                case 0xD: { // MUL
                    const accel_sym b = rd_sym(rm);
                    const accel_sym a = rd_sym(rd);
                    if (a.kind == accel_sym::S_CONST && b.kind == accel_sym::S_CONST)
                        wr_sym(rd, accel_sym::make_const(a.c * b.c));
                    else
                        wr_sym(rd, accel_sym::make_top());
                    last_flag = FLAG_OTHER;
                    break;
                }
                case 0xE: { // BIC
                    const accel_sym b = rd_sym(rm);
                    const accel_sym a = rd_sym(rd);
                    if (b.kind == accel_sym::S_CONST)
                        wr_sym(rd, apply_const(a, ACC_AND, ~b.c));
                    else
                        wr_sym(rd, accel_sym::make_top());
                    last_flag = FLAG_OTHER;
                    break;
                }
                case 0xF: { // MVN
                    const accel_sym b = rd_sym(rm);
                    if (b.kind == accel_sym::S_CONST)
                        wr_sym(rd, accel_sym::make_const(~b.c));
                    else
                        wr_sym(rd, apply_const(b, ACC_EOR, 0xFFFFFFFFu));
                    last_flag = FLAG_OTHER;
                    break;
                }
                default:
                    return false; // ADC/SBC/ROR read or thread flags
                }
            } else if ((hw & 0xF800) == 0x4800) {
                return false; // LDR literal: loop-invariant unknown, v1 rejects
            } else if ((hw & 0xFC00) == 0x4400) {
                return false; // hi-reg ops / BX
            } else { // 0101: reg-offset load/store
                const int opk = (hw >> 9) & 7;
                const int rm = (hw >> 6) & 7, rn = (hw >> 3) & 7, rd = hw & 7;
                const accel_sym bsym = rd_sym(rn);
                const accel_sym osym = rd_sym(rm);
                accel_sym addr;
                if (bsym.kind == accel_sym::S_AFFINE && osym.kind == accel_sym::S_CONST)
                    addr = apply_const(bsym, ACC_ADD, osym.c);
                else if (osym.kind == accel_sym::S_AFFINE && bsym.kind == accel_sym::S_CONST)
                    addr = apply_const(osym, ACC_ADD, bsym.c);
                else
                    return false;
                // opk: 0 STR,1 STRH,2 STRB,3 LDRSB,4 LDR,5 LDRH,6 LDRB,7 LDRSH
                static const std::uint8_t widths[8] = { 4, 2, 1, 1, 4, 2, 1, 2 };
                const bool is_load = opk >= 3;
                const std::uint8_t w = widths[opk];
                if (is_load) {
                    if (have_load || store_seen)
                        return false;
                    have_load = true;
                    load_width = w;
                    load_signed = (opk == 3 || opk == 7);
                    src_reg = addr.reg;
                    src_off = static_cast<std::int32_t>(addr.c);
                    accel_sym lv{};
                    lv.kind = accel_sym::S_CHAIN;
                    if (load_signed && !lv.push_op(w == 1 ? ACC_SXTB : ACC_SXTH, 0))
                        return false;
                    wr_sym(rd, lv);
                } else {
                    if (store_count >= 4)
                        return false;
                    const accel_sym v = rd_sym(rd);
                    if (v.kind == accel_sym::S_TOP)
                        return false;
                    raw_stores[store_count].base = addr.reg;
                    raw_stores[store_count].off = static_cast<std::int32_t>(addr.c);
                    raw_stores[store_count].width = w;
                    raw_stores[store_count].val = v;
                    store_count++;
                    store_seen = true;
                }
                // Memory ops leave the flags untouched: keep last_flag as-is.
            }
            break;
        }
        case 3: // 011: LDR/STR/LDRB/STRB imm5
        case 4: { // 100: LDRH/STRH imm5 / SP-relative
            std::uint8_t w;
            bool is_load;
            std::uint32_t imm_off;
            const int rn = (hw >> 3) & 7, rd = hw & 7;
            if ((hw >> 13) == 3) {
                const bool byte = (hw >> 12) & 1;
                is_load = (hw >> 11) & 1;
                w = byte ? 1 : 4;
                imm_off = ((hw >> 6) & 31) * (byte ? 1u : 4u);
            } else {
                if ((hw >> 12) & 1)
                    return false; // 1001x: SP-relative
                is_load = (hw >> 11) & 1;
                w = 2;
                imm_off = ((hw >> 6) & 31) * 2u;
            }
            const accel_sym bsym = rd_sym(rn);
            if (bsym.kind != accel_sym::S_AFFINE)
                return false;
            const std::int32_t off = static_cast<std::int32_t>(bsym.c + imm_off);
            if (is_load) {
                if (have_load || store_seen)
                    return false;
                have_load = true;
                load_width = w;
                load_signed = 0;
                src_reg = bsym.reg;
                src_off = off;
                accel_sym lv{};
                lv.kind = accel_sym::S_CHAIN;
                wr_sym(rd, lv);
            } else {
                if (store_count >= 4)
                    return false;
                const accel_sym v = rd_sym(rd);
                if (v.kind == accel_sym::S_TOP)
                    return false;
                raw_stores[store_count].base = bsym.reg;
                raw_stores[store_count].off = off;
                raw_stores[store_count].width = w;
                raw_stores[store_count].val = v;
                store_count++;
                store_seen = true;
            }
            break;
        }
        default:
            return false; // SP-ops, misc, LDM/STM, unconditional B, BL, ...
        }

        pc += 2;
    }

    if (!matched_branch || store_count == 0)
        return false;

    // Loop condition: flags at the branch must come from CMP rc,#0 or
    // SUBS rc,#1 with rc ending the body at INIT(rc)-1.
    if ((last_flag != FLAG_CMP0 && last_flag != FLAG_SUB1) || flag_reg < 0)
        return false;
    const int rc = flag_reg;
    if (sym[rc].kind != accel_sym::S_AFFINE || sym[rc].reg != rc || sym[rc].c != 0xFFFFFFFFu)
        return false;

    // Uniformity: every register read before written must end the body as
    // INIT(self) + constant delta.
    std::int32_t deltas[8];
    for (int r = 0; r < 8; r++) {
        deltas[r] = 0;
        if (first_access[r] == ACC_READ) {
            if (sym[r].kind != accel_sym::S_AFFINE || sym[r].reg != r)
                return false;
            deltas[r] = static_cast<std::int32_t>(sym[r].c);
        }
    }

    // Stores: single dst base with positive step, dense byte tiling.
    const std::uint8_t q = raw_stores[0].base;
    std::int32_t min_off = raw_stores[0].off;
    for (int i = 0; i < store_count; i++) {
        if (raw_stores[i].base != q)
            return false;
        min_off = std::min(min_off, raw_stores[i].off);
    }
    if (first_access[q] != ACC_READ)
        return false;
    const std::int32_t dst_step = deltas[q];
    std::uint32_t covered = 0;
    std::uint32_t span_bytes = 0;
    for (int i = 0; i < store_count; i++) {
        const std::uint32_t rel = static_cast<std::uint32_t>(raw_stores[i].off - min_off);
        if (rel + raw_stores[i].width > 32)
            return false;
        const std::uint32_t mask = ((raw_stores[i].width == 4) ? 0xFu : (raw_stores[i].width == 2) ? 0x3u
                                                                                                   : 0x1u)
            << rel;
        if (covered & mask)
            return false; // overlapping bytes: order-dependent, reject
        covered |= mask;
        span_bytes = std::max(span_bytes, rel + raw_stores[i].width);
    }
    if (covered != ((span_bytes >= 32) ? 0xFFFFFFFFu : ((1u << span_bytes) - 1)))
        return false; // gaps in the written span
    if (span_bytes > 8)
        return false;
    if (dst_step < static_cast<std::int32_t>(span_bytes))
        return false; // must advance past what it wrote (also rejects step<=0)

    // Load: positive stride.
    if (have_load) {
        if (first_access[src_reg] != ACC_READ)
            return false;
        if (deltas[src_reg] <= 0)
            return false;
    }

    // Store values: only const / chain-of-load / invariant-register.
    for (int i = 0; i < store_count; i++) {
        const accel_sym &v = raw_stores[i].val;
        if (v.kind == accel_sym::S_CHAIN && !have_load)
            return false;
        if (v.kind == accel_sym::S_AFFINE && deltas[v.reg] != 0)
            return false;
    }

    // Build the descriptor.
    out = loop_accel_inst{};
    out.has_load = have_load ? 1 : 0;
    out.load_width = load_width;
    out.load_signed = load_signed;
    out.src_reg = src_reg;
    out.src_off = src_off;
    out.src_step = have_load ? deltas[src_reg] : 0;
    out.dst_reg = q;
    out.dst_min_off = min_off;
    out.dst_step = dst_step;
    out.span = static_cast<std::uint8_t>(span_bytes);
    out.store_count = static_cast<std::uint8_t>(store_count);
    for (int i = 0; i < store_count; i++) {
        accel_store &s = out.stores[i];
        s.off = raw_stores[i].off;
        s.width = raw_stores[i].width;
        const accel_sym &v = raw_stores[i].val;
        if (v.kind == accel_sym::S_CONST) {
            s.kind = ACC_VAL_CONST;
            s.cval = v.c;
        } else if (v.kind == accel_sym::S_AFFINE) {
            s.kind = ACC_VAL_REG;
            s.reg = v.reg;
            s.cval = v.c;
        } else {
            s.kind = ACC_VAL_CHAIN;
            s.nops = v.nops;
            for (int k = 0; k < v.nops; k++) {
                s.ops[k].op = v.ops[k].op;
                s.ops[k].imm = v.ops[k].imm;
            }
        }
    }
    out.ind_count = 0;
    for (int r = 0; r < 8; r++) {
        if (first_access[r] == ACC_READ && deltas[r] != 0) {
            if (out.ind_count >= 8)
                return false;
            out.ind[out.ind_count].reg = static_cast<std::uint8_t>(r);
            out.ind[out.ind_count].delta = deltas[r];
            out.ind_count++;
        }
    }
    out.counter_reg = static_cast<std::uint8_t>(rc);
    out.body_len = static_cast<std::uint16_t>(count);
    return true;
}

// Execute up to `want` full iterations natively through the dyncom TLB.
// Returns the number of iterations actually performed; the caller advances
// the induction registers by that count. Stops early at any TLB miss so the
// interpreter (and its slow path / fault behaviour) takes over exactly where
// the bulk run left off.
static std::uint32_t run_accel_bulk(ARMul_State *cpu, const loop_accel_inst *d, std::uint32_t want) {
    eka2l1::arm::r12l1::tlb *tlb = cpu->mem_cache_;
    const std::uint32_t page_size = static_cast<std::uint32_t>(tlb->page_mask) + 1;

    std::uint32_t src = d->has_load ? (cpu->Reg[d->src_reg] + d->src_off) : 0;
    std::uint32_t dst = cpu->Reg[d->dst_reg] + d->dst_min_off;

    // Pre-resolve invariant register store values.
    std::uint32_t reg_vals[4];
    for (int i = 0; i < d->store_count; i++)
        reg_vals[i] = (d->stores[i].kind == ACC_VAL_REG)
            ? (cpu->Reg[d->stores[i].reg] + d->stores[i].cval)
            : d->stores[i].cval;

    std::uint32_t done = 0;
    while (done < want) {
        std::uint8_t *hs = nullptr;
        std::uint32_t it = want - done;

        if (d->has_load) {
            hs = tlb->lookup(src);
            if (!hs)
                break;
            const std::uint32_t room = page_size - (src & static_cast<std::uint32_t>(tlb->page_mask));
            if (room < d->load_width)
                break;
            it = std::min<std::uint32_t>(it, (room - d->load_width) / static_cast<std::uint32_t>(d->src_step) + 1);
        }

        std::uint8_t *hd = tlb->lookup(dst);
        if (!hd)
            break;
        const std::uint32_t room_d = page_size - (dst & static_cast<std::uint32_t>(tlb->page_mask));
        if (room_d < d->span)
            break;
        it = std::min<std::uint32_t>(it, (room_d - d->span) / static_cast<std::uint32_t>(d->dst_step) + 1);
        if (!it)
            break;

        for (std::uint32_t i = 0; i < it; i++) {
            std::uint32_t lv = 0;
            if (d->has_load) {
                switch (d->load_width) {
                case 1:
                    lv = *hs;
                    break;
                case 2: {
                    std::uint16_t t;
                    std::memcpy(&t, hs, 2);
                    lv = t;
                    break;
                }
                default: {
                    std::memcpy(&lv, hs, 4);
                    break;
                }
                }
                hs += d->src_step;
            }
            for (int s = 0; s < d->store_count; s++) {
                const accel_store &st = d->stores[s];
                std::uint32_t v;
                if (st.kind == ACC_VAL_CHAIN) {
                    v = lv;
                    for (int k = 0; k < st.nops; k++)
                        v = accel_apply_op(v, st.ops[k].op, st.ops[k].imm);
                } else {
                    v = reg_vals[s];
                }
                std::uint8_t *p = hd + (st.off - d->dst_min_off);
                switch (st.width) {
                case 1:
                    *p = static_cast<std::uint8_t>(v);
                    break;
                case 2: {
                    const std::uint16_t t = static_cast<std::uint16_t>(v);
                    std::memcpy(p, &t, 2);
                    break;
                }
                default:
                    std::memcpy(p, &v, 4);
                    break;
                }
            }
            hd += d->dst_step;
        }

        src += it * static_cast<std::uint32_t>(d->src_step);
        dst += it * static_cast<std::uint32_t>(d->dst_step);
        done += it;
    }
#if defined(EKA2L1_DYNCOM_DIFFTEST)
    g_loop_accel_bulk_iterations += done;
#endif
    return done;
}

static unsigned int DPO(Immediate)(ARMul_State *cpu, unsigned int sht_oper) {
    unsigned int immed_8 = BITS(sht_oper, 0, 7);
    unsigned int rotate_imm = BITS(sht_oper, 8, 11);
    unsigned int shifter_operand = ROTATE_RIGHT_32(immed_8, rotate_imm * 2);
    if (rotate_imm == 0)
        cpu->shifter_carry_out = cpu->CFlag;
    else
        cpu->shifter_carry_out = BIT(shifter_operand, 31);
    return shifter_operand;
}

static unsigned int DPO(Register)(ARMul_State *cpu, unsigned int sht_oper) {
    unsigned int rm = CHECK_READ_REG15(cpu, RM);
    unsigned int shifter_operand = rm;
    cpu->shifter_carry_out = cpu->CFlag;
    return shifter_operand;
}

static unsigned int DPO(LogicalShiftLeftByImmediate)(ARMul_State *cpu, unsigned int sht_oper) {
    int shift_imm = BITS(sht_oper, 7, 11);
    unsigned int rm = CHECK_READ_REG15(cpu, RM);
    unsigned int shifter_operand;
    if (shift_imm == 0) {
        shifter_operand = rm;
        cpu->shifter_carry_out = cpu->CFlag;
    } else {
        shifter_operand = rm << shift_imm;
        cpu->shifter_carry_out = BIT(rm, 32 - shift_imm);
    }
    return shifter_operand;
}

static unsigned int DPO(LogicalShiftLeftByRegister)(ARMul_State *cpu, unsigned int sht_oper) {
    int shifter_operand;
    unsigned int rm = CHECK_READ_REG15(cpu, RM);
    unsigned int rs = CHECK_READ_REG15(cpu, RS);
    if (BITS(rs, 0, 7) == 0) {
        shifter_operand = rm;
        cpu->shifter_carry_out = cpu->CFlag;
    } else if (BITS(rs, 0, 7) < 32) {
        shifter_operand = rm << BITS(rs, 0, 7);
        cpu->shifter_carry_out = BIT(rm, 32 - BITS(rs, 0, 7));
    } else if (BITS(rs, 0, 7) == 32) {
        shifter_operand = 0;
        cpu->shifter_carry_out = BIT(rm, 0);
    } else {
        shifter_operand = 0;
        cpu->shifter_carry_out = 0;
    }
    return shifter_operand;
}

static unsigned int DPO(LogicalShiftRightByImmediate)(ARMul_State *cpu, unsigned int sht_oper) {
    unsigned int rm = CHECK_READ_REG15(cpu, RM);
    unsigned int shifter_operand;
    int shift_imm = BITS(sht_oper, 7, 11);
    if (shift_imm == 0) {
        shifter_operand = 0;
        cpu->shifter_carry_out = BIT(rm, 31);
    } else {
        shifter_operand = rm >> shift_imm;
        cpu->shifter_carry_out = BIT(rm, shift_imm - 1);
    }
    return shifter_operand;
}

static unsigned int DPO(LogicalShiftRightByRegister)(ARMul_State *cpu, unsigned int sht_oper) {
    unsigned int rs = CHECK_READ_REG15(cpu, RS);
    unsigned int rm = CHECK_READ_REG15(cpu, RM);
    unsigned int shifter_operand;
    if (BITS(rs, 0, 7) == 0) {
        shifter_operand = rm;
        cpu->shifter_carry_out = cpu->CFlag;
    } else if (BITS(rs, 0, 7) < 32) {
        shifter_operand = rm >> BITS(rs, 0, 7);
        cpu->shifter_carry_out = BIT(rm, BITS(rs, 0, 7) - 1);
    } else if (BITS(rs, 0, 7) == 32) {
        shifter_operand = 0;
        cpu->shifter_carry_out = BIT(rm, 31);
    } else {
        shifter_operand = 0;
        cpu->shifter_carry_out = 0;
    }
    return shifter_operand;
}

static unsigned int DPO(ArithmeticShiftRightByImmediate)(ARMul_State *cpu, unsigned int sht_oper) {
    unsigned int rm = CHECK_READ_REG15(cpu, RM);
    unsigned int shifter_operand;
    int shift_imm = BITS(sht_oper, 7, 11);
    if (shift_imm == 0) {
        if (BIT(rm, 31) == 0)
            shifter_operand = 0;
        else
            shifter_operand = 0xFFFFFFFF;
        cpu->shifter_carry_out = BIT(rm, 31);
    } else {
        shifter_operand = static_cast<int>(rm) >> shift_imm;
        cpu->shifter_carry_out = BIT(rm, shift_imm - 1);
    }
    return shifter_operand;
}

static unsigned int DPO(ArithmeticShiftRightByRegister)(ARMul_State *cpu, unsigned int sht_oper) {
    unsigned int rs = CHECK_READ_REG15(cpu, RS);
    unsigned int rm = CHECK_READ_REG15(cpu, RM);
    unsigned int shifter_operand;
    if (BITS(rs, 0, 7) == 0) {
        shifter_operand = rm;
        cpu->shifter_carry_out = cpu->CFlag;
    } else if (BITS(rs, 0, 7) < 32) {
        shifter_operand = static_cast<int>(rm) >> BITS(rs, 0, 7);
        cpu->shifter_carry_out = BIT(rm, BITS(rs, 0, 7) - 1);
    } else {
        if (BIT(rm, 31) == 0)
            shifter_operand = 0;
        else
            shifter_operand = 0xffffffff;
        cpu->shifter_carry_out = BIT(rm, 31);
    }
    return shifter_operand;
}

static unsigned int DPO(RotateRightByImmediate)(ARMul_State *cpu, unsigned int sht_oper) {
    unsigned int shifter_operand;
    unsigned int rm = CHECK_READ_REG15(cpu, RM);
    int shift_imm = BITS(sht_oper, 7, 11);
    if (shift_imm == 0) {
        shifter_operand = (cpu->CFlag << 31) | (rm >> 1);
        cpu->shifter_carry_out = BIT(rm, 0);
    } else {
        shifter_operand = ROTATE_RIGHT_32(rm, shift_imm);
        cpu->shifter_carry_out = BIT(rm, shift_imm - 1);
    }
    return shifter_operand;
}

static unsigned int DPO(RotateRightByRegister)(ARMul_State *cpu, unsigned int sht_oper) {
    unsigned int rm = CHECK_READ_REG15(cpu, RM);
    unsigned int rs = CHECK_READ_REG15(cpu, RS);
    unsigned int shifter_operand;
    if (BITS(rs, 0, 7) == 0) {
        shifter_operand = rm;
        cpu->shifter_carry_out = cpu->CFlag;
    } else if (BITS(rs, 0, 4) == 0) {
        shifter_operand = rm;
        cpu->shifter_carry_out = BIT(rm, 31);
    } else {
        shifter_operand = ROTATE_RIGHT_32(rm, BITS(rs, 0, 4));
        cpu->shifter_carry_out = BIT(rm, BITS(rs, 0, 4) - 1);
    }
    return shifter_operand;
}

// Inlined shifter-operand evaluation. This used to be a statement expression so
// it could stay usable where a plain expression was expected; MSVC has no such
// extension, and a force-inlined function does the same job everywhere.
static DYNCOM_FORCE_INLINE unsigned int compute_shifter_operand(ARMul_State *cpu,
    const shtop_fp_t f_, const unsigned int so_) {
    unsigned int v_;
    if (f_ == DataProcessingOperandsImmediate) {
        v_ = ROTATE_RIGHT_32(BITS(so_, 0, 7), BITS(so_, 8, 11) * 2);
        cpu->shifter_carry_out = (BITS(so_, 8, 11) == 0) ? cpu->CFlag : BIT(v_, 31);
    } else if (f_ == DataProcessingOperandsRegister) {
        v_ = CHECK_READ_REG15(cpu, BITS(so_, 0, 3));
        cpu->shifter_carry_out = cpu->CFlag;
    } else if (f_ == DataProcessingOperandsLogicalShiftLeftByImmediate) {
        const unsigned int rm_ = CHECK_READ_REG15(cpu, BITS(so_, 0, 3));
        const unsigned int imm_ = BITS(so_, 7, 11);
        if (imm_ == 0) {
            v_ = rm_;
            cpu->shifter_carry_out = cpu->CFlag;
        } else {
            v_ = rm_ << imm_;
            cpu->shifter_carry_out = BIT(rm_, 32 - imm_);
        }
    } else if (f_ == DataProcessingOperandsArithmeticShiftRightByImmediate) {
        const unsigned int rm_ = CHECK_READ_REG15(cpu, BITS(so_, 0, 3));
        const unsigned int imm_ = BITS(so_, 7, 11);
        if (imm_ == 0) {
            v_ = BIT(rm_, 31) ? 0xFFFFFFFF : 0;
            cpu->shifter_carry_out = BIT(rm_, 31);
        } else {
            v_ = static_cast<unsigned int>(static_cast<int>(rm_) >> imm_);
            cpu->shifter_carry_out = BIT(rm_, imm_ - 1);
        }
    } else if (f_ == DataProcessingOperandsLogicalShiftRightByImmediate) {
        const unsigned int rm_ = CHECK_READ_REG15(cpu, BITS(so_, 0, 3));
        const unsigned int imm_ = BITS(so_, 7, 11);
        if (imm_ == 0) {
            v_ = 0;
            cpu->shifter_carry_out = BIT(rm_, 31);
        } else {
            v_ = rm_ >> imm_;
            cpu->shifter_carry_out = BIT(rm_, imm_ - 1);
        }
    } else {
        v_ = f_(cpu, so_);
    }
    return v_;
}


#define DEBUG_MSG                                        \
    LOG_DEBUG(eka2l1::CPU_DYNCOM, "inst is {:x}", inst); \
    CITRA_IGNORE_EXIT(0)

#define LnSWoUB(s) glue(LnSWoUB, s)
#define MLnS(s) glue(MLnS, s)
#define LdnStM(s) glue(LdnStM, s)

#define W_BIT BIT(inst, 21)
#define U_BIT BIT(inst, 23)
#define I_BIT BIT(inst, 25)
#define P_BIT BIT(inst, 24)
#define OFFSET_12 BITS(inst, 0, 11)

static void LnSWoUB(ImmediateOffset)(ARMul_State *cpu, unsigned int inst, unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int addr;

    if (U_BIT)
        addr = CHECK_READ_REG15_WA(cpu, Rn) + OFFSET_12;
    else
        addr = CHECK_READ_REG15_WA(cpu, Rn) - OFFSET_12;

    virt_addr = addr;
}

// Fast path for the dominant single load/store addressing form: immediate
// offset, no write-back (`[Rn, #+/-imm12]`). The generic path calls
// inst_cream->get_addr through a function pointer -- a polymorphic indirect
// branch at the shared handler site. This inlines the trivial base +/- imm12
// computation for that common form and only falls back to the pointer for the
// rarer modes; the pointer compare is one well-predicted branch.
#define LS_GET_ADDR(addr_out)                                                        \
    do {                                                                             \
        if (inst_cream->get_addr == LnSWoUBImmediateOffset) {                        \
            const unsigned int ls_inst_ = inst_cream->inst;                          \
            const std::uint32_t ls_base_ =                                           \
                CHECK_READ_REG15_WA(cpu, BITS(ls_inst_, 16, 19));                    \
            (addr_out) = BIT(ls_inst_, 23) ? (ls_base_ + BITS(ls_inst_, 0, 11))      \
                                           : (ls_base_ - BITS(ls_inst_, 0, 11));     \
        } else if (inst_cream->get_addr == LnSWoUBRegisterOffset) {                  \
            const unsigned int ls_inst_ = inst_cream->inst;                          \
            const std::uint32_t ls_base_ =                                           \
                CHECK_READ_REG15_WA(cpu, BITS(ls_inst_, 16, 19));                    \
            const std::uint32_t ls_off_ =                                            \
                CHECK_READ_REG15_WA(cpu, BITS(ls_inst_, 0, 3));                      \
            (addr_out) = BIT(ls_inst_, 23) ? (ls_base_ + ls_off_)                    \
                                           : (ls_base_ - ls_off_);                   \
        } else if (inst_cream->get_addr) {                                           \
            inst_cream->get_addr(cpu, inst_cream->inst, (addr_out));                 \
        } else {                                                                     \
            undef_inst = inst_cream->inst;                                           \
            goto UNDEFINED_ADDRESSING_MODE;                                          \
        }                                                                            \
    } while (0)

// LS_GET_ADDR for the miscellaneous (halfword/doubleword) load/store family,
// whose dominant addressing forms are the split-immediate and register
// offsets computed by MLnSImmediateOffset / MLnSRegisterOffset.
#define MLS_GET_ADDR(addr_out)                                                       \
    do {                                                                             \
        if (inst_cream->get_addr == MLnSImmediateOffset) {                           \
            const unsigned int ls_inst_ = inst_cream->inst;                          \
            const std::uint32_t ls_base_ =                                           \
                CHECK_READ_REG15_WA(cpu, BITS(ls_inst_, 16, 19));                    \
            const std::uint32_t ls_off_ =                                            \
                (BITS(ls_inst_, 8, 11) << 4) | BITS(ls_inst_, 0, 3);                 \
            (addr_out) = BIT(ls_inst_, 23) ? (ls_base_ + ls_off_)                    \
                                           : (ls_base_ - ls_off_);                   \
        } else if (inst_cream->get_addr == MLnSRegisterOffset) {                     \
            const unsigned int ls_inst_ = inst_cream->inst;                          \
            const std::uint32_t ls_base_ =                                           \
                CHECK_READ_REG15_WA(cpu, BITS(ls_inst_, 16, 19));                    \
            const std::uint32_t ls_off_ =                                            \
                CHECK_READ_REG15_WA(cpu, BITS(ls_inst_, 0, 3));                      \
            (addr_out) = BIT(ls_inst_, 23) ? (ls_base_ + ls_off_)                    \
                                           : (ls_base_ - ls_off_);                   \
        } else if (inst_cream->get_addr) {                                           \
            inst_cream->get_addr(cpu, inst_cream->inst, (addr_out));                 \
        } else {                                                                     \
            undef_inst = inst_cream->inst;                                           \
            goto UNDEFINED_ADDRESSING_MODE;                                          \
        }                                                                            \
    } while (0)

static void LnSWoUB(RegisterOffset)(ARMul_State *cpu, unsigned int inst, unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int Rm = BITS(inst, 0, 3);
    unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
    unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
    unsigned int addr;

    if (U_BIT)
        addr = rn + rm;
    else
        addr = rn - rm;

    virt_addr = addr;
}

static void LnSWoUB(ImmediatePostIndexed)(ARMul_State *cpu, unsigned int inst,
    unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int addr = CHECK_READ_REG15_WA(cpu, Rn);

    if (U_BIT)
        cpu->Reg[Rn] += OFFSET_12;
    else
        cpu->Reg[Rn] -= OFFSET_12;

    virt_addr = addr;
}

static void LnSWoUB(ImmediatePreIndexed)(ARMul_State *cpu, unsigned int inst,
    unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int addr;

    if (U_BIT)
        addr = CHECK_READ_REG15_WA(cpu, Rn) + OFFSET_12;
    else
        addr = CHECK_READ_REG15_WA(cpu, Rn) - OFFSET_12;

    virt_addr = addr;

    if (CondPassed(cpu, BITS(inst, 28, 31)))
        cpu->Reg[Rn] = addr;
}

static void MLnS(RegisterPreIndexed)(ARMul_State *cpu, unsigned int inst, unsigned int &virt_addr) {
    unsigned int addr;
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int Rm = BITS(inst, 0, 3);
    unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
    unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);

    if (U_BIT)
        addr = rn + rm;
    else
        addr = rn - rm;

    virt_addr = addr;

    if (CondPassed(cpu, BITS(inst, 28, 31)))
        cpu->Reg[Rn] = addr;
}

static void LnSWoUB(RegisterPreIndexed)(ARMul_State *cpu, unsigned int inst,
    unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int Rm = BITS(inst, 0, 3);
    unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
    unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
    unsigned int addr;

    if (U_BIT)
        addr = rn + rm;
    else
        addr = rn - rm;

    virt_addr = addr;

    if (CondPassed(cpu, BITS(inst, 28, 31))) {
        cpu->Reg[Rn] = addr;
    }
}

static void LnSWoUB(ScaledRegisterPreIndexed)(ARMul_State *cpu, unsigned int inst,
    unsigned int &virt_addr) {
    unsigned int shift = BITS(inst, 5, 6);
    unsigned int shift_imm = BITS(inst, 7, 11);
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int Rm = BITS(inst, 0, 3);
    unsigned int index = 0;
    unsigned int addr;
    unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
    unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);

    switch (shift) {
    case 0:
        index = rm << shift_imm;
        break;
    case 1:
        if (shift_imm == 0) {
            index = 0;
        } else {
            index = rm >> shift_imm;
        }
        break;
    case 2:
        if (shift_imm == 0) { // ASR #32
            if (BIT(rm, 31) == 1)
                index = 0xFFFFFFFF;
            else
                index = 0;
        } else {
            index = static_cast<int>(rm) >> shift_imm;
        }
        break;
    case 3:
        if (shift_imm == 0) {
            index = (cpu->CFlag << 31) | (rm >> 1);
        } else {
            index = ROTATE_RIGHT_32(rm, shift_imm);
        }
        break;
    }

    if (U_BIT)
        addr = rn + index;
    else
        addr = rn - index;

    virt_addr = addr;

    if (CondPassed(cpu, BITS(inst, 28, 31)))
        cpu->Reg[Rn] = addr;
}

static void LnSWoUB(ScaledRegisterPostIndexed)(ARMul_State *cpu, unsigned int inst,
    unsigned int &virt_addr) {
    unsigned int shift = BITS(inst, 5, 6);
    unsigned int shift_imm = BITS(inst, 7, 11);
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int Rm = BITS(inst, 0, 3);
    unsigned int index = 0;
    unsigned int addr = CHECK_READ_REG15_WA(cpu, Rn);
    unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);

    switch (shift) {
    case 0:
        index = rm << shift_imm;
        break;
    case 1:
        if (shift_imm == 0) {
            index = 0;
        } else {
            index = rm >> shift_imm;
        }
        break;
    case 2:
        if (shift_imm == 0) { // ASR #32
            if (BIT(rm, 31) == 1)
                index = 0xFFFFFFFF;
            else
                index = 0;
        } else {
            index = static_cast<int>(rm) >> shift_imm;
        }
        break;
    case 3:
        if (shift_imm == 0) {
            index = (cpu->CFlag << 31) | (rm >> 1);
        } else {
            index = ROTATE_RIGHT_32(rm, shift_imm);
        }
        break;
    }

    virt_addr = addr;

    if (CondPassed(cpu, BITS(inst, 28, 31))) {
        if (U_BIT)
            cpu->Reg[Rn] += index;
        else
            cpu->Reg[Rn] -= index;
    }
}

static void LnSWoUB(RegisterPostIndexed)(ARMul_State *cpu, unsigned int inst,
    unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int Rm = BITS(inst, 0, 3);
    unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);

    virt_addr = CHECK_READ_REG15_WA(cpu, Rn);

    if (CondPassed(cpu, BITS(inst, 28, 31))) {
        if (U_BIT) {
            cpu->Reg[Rn] += rm;
        } else {
            cpu->Reg[Rn] -= rm;
        }
    }
}

static void MLnS(ImmediateOffset)(ARMul_State *cpu, unsigned int inst, unsigned int &virt_addr) {
    unsigned int immedL = BITS(inst, 0, 3);
    unsigned int immedH = BITS(inst, 8, 11);
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int addr;

    unsigned int offset_8 = (immedH << 4) | immedL;

    if (U_BIT)
        addr = CHECK_READ_REG15_WA(cpu, Rn) + offset_8;
    else
        addr = CHECK_READ_REG15_WA(cpu, Rn) - offset_8;

    virt_addr = addr;
}

static void MLnS(RegisterOffset)(ARMul_State *cpu, unsigned int inst, unsigned int &virt_addr) {
    unsigned int addr;
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int Rm = BITS(inst, 0, 3);
    unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
    unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);

    if (U_BIT)
        addr = rn + rm;
    else
        addr = rn - rm;

    virt_addr = addr;
}

static void MLnS(ImmediatePreIndexed)(ARMul_State *cpu, unsigned int inst,
    unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int immedH = BITS(inst, 8, 11);
    unsigned int immedL = BITS(inst, 0, 3);
    unsigned int addr;
    unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
    unsigned int offset_8 = (immedH << 4) | immedL;

    if (U_BIT)
        addr = rn + offset_8;
    else
        addr = rn - offset_8;

    virt_addr = addr;

    if (CondPassed(cpu, BITS(inst, 28, 31)))
        cpu->Reg[Rn] = addr;
}

static void MLnS(ImmediatePostIndexed)(ARMul_State *cpu, unsigned int inst,
    unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int immedH = BITS(inst, 8, 11);
    unsigned int immedL = BITS(inst, 0, 3);
    unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);

    virt_addr = rn;

    if (CondPassed(cpu, BITS(inst, 28, 31))) {
        unsigned int offset_8 = (immedH << 4) | immedL;
        if (U_BIT)
            rn += offset_8;
        else
            rn -= offset_8;

        cpu->Reg[Rn] = rn;
    }
}

static void MLnS(RegisterPostIndexed)(ARMul_State *cpu, unsigned int inst,
    unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int Rm = BITS(inst, 0, 3);
    unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);

    virt_addr = CHECK_READ_REG15_WA(cpu, Rn);

    if (CondPassed(cpu, BITS(inst, 28, 31))) {
        if (U_BIT)
            cpu->Reg[Rn] += rm;
        else
            cpu->Reg[Rn] -= rm;
    }
}

// Register count of an LDM/STM register list, replacing the former 1..16
// iteration shift loop executed on every block transfer.
static inline int CountSetBits16(unsigned int v) {
    return std::popcount(v & 0xFFFFu);
}

static void LdnStM(DecrementBefore)(ARMul_State *cpu, unsigned int inst, unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    const int count = CountSetBits16(BITS(inst, 0, 15));

    virt_addr = CHECK_READ_REG15_WA(cpu, Rn) - count * 4;

    if (CondPassed(cpu, BITS(inst, 28, 31)) && BIT(inst, 21))
        cpu->Reg[Rn] -= count * 4;
}

static void LdnStM(IncrementBefore)(ARMul_State *cpu, unsigned int inst, unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    const int count = CountSetBits16(BITS(inst, 0, 15));

    virt_addr = CHECK_READ_REG15_WA(cpu, Rn) + 4;

    if (CondPassed(cpu, BITS(inst, 28, 31)) && BIT(inst, 21))
        cpu->Reg[Rn] += count * 4;
}

static void LdnStM(IncrementAfter)(ARMul_State *cpu, unsigned int inst, unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    const int count = CountSetBits16(BITS(inst, 0, 15));

    virt_addr = CHECK_READ_REG15_WA(cpu, Rn);

    if (CondPassed(cpu, BITS(inst, 28, 31)) && BIT(inst, 21))
        cpu->Reg[Rn] += count * 4;
}

static void LdnStM(DecrementAfter)(ARMul_State *cpu, unsigned int inst, unsigned int &virt_addr) {
    unsigned int Rn = BITS(inst, 16, 19);
    const int count = CountSetBits16(BITS(inst, 0, 15));
    unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);
    unsigned int start_addr = rn - count * 4 + 4;

    virt_addr = start_addr;

    if (CondPassed(cpu, BITS(inst, 28, 31)) && BIT(inst, 21)) {
        cpu->Reg[Rn] -= count * 4;
    }
}

static void LnSWoUB(ScaledRegisterOffset)(ARMul_State *cpu, unsigned int inst,
    unsigned int &virt_addr) {
    unsigned int shift = BITS(inst, 5, 6);
    unsigned int shift_imm = BITS(inst, 7, 11);
    unsigned int Rn = BITS(inst, 16, 19);
    unsigned int Rm = BITS(inst, 0, 3);
    unsigned int index = 0;
    unsigned int addr;
    unsigned int rm = CHECK_READ_REG15_WA(cpu, Rm);
    unsigned int rn = CHECK_READ_REG15_WA(cpu, Rn);

    switch (shift) {
    case 0:
        index = rm << shift_imm;
        break;
    case 1:
        if (shift_imm == 0) {
            index = 0;
        } else {
            index = rm >> shift_imm;
        }
        break;
    case 2:
        if (shift_imm == 0) { // ASR #32
            if (BIT(rm, 31) == 1)
                index = 0xFFFFFFFF;
            else
                index = 0;
        } else {
            index = static_cast<int>(rm) >> shift_imm;
        }
        break;
    case 3:
        if (shift_imm == 0) {
            index = (cpu->CFlag << 31) | (rm >> 1);
        } else {
            index = ROTATE_RIGHT_32(rm, shift_imm);
        }
        break;
    }

    if (U_BIT) {
        addr = rn + index;
    } else
        addr = rn - index;

    virt_addr = addr;
}

shtop_fp_t GetShifterOp(unsigned int inst) {
    if (BIT(inst, 25)) {
        return DPO(Immediate);
    } else if (BITS(inst, 4, 11) == 0) {
        return DPO(Register);
    } else if (BITS(inst, 4, 6) == 0) {
        return DPO(LogicalShiftLeftByImmediate);
    } else if (BITS(inst, 4, 7) == 1) {
        return DPO(LogicalShiftLeftByRegister);
    } else if (BITS(inst, 4, 6) == 2) {
        return DPO(LogicalShiftRightByImmediate);
    } else if (BITS(inst, 4, 7) == 3) {
        return DPO(LogicalShiftRightByRegister);
    } else if (BITS(inst, 4, 6) == 4) {
        return DPO(ArithmeticShiftRightByImmediate);
    } else if (BITS(inst, 4, 7) == 5) {
        return DPO(ArithmeticShiftRightByRegister);
    } else if (BITS(inst, 4, 6) == 6) {
        return DPO(RotateRightByImmediate);
    } else if (BITS(inst, 4, 7) == 7) {
        return DPO(RotateRightByRegister);
    }
    return nullptr;
}

get_addr_fp_t GetAddressingOp(unsigned int inst) {
    if (BITS(inst, 24, 27) == 5 && BIT(inst, 21) == 0) {
        return LnSWoUB(ImmediateOffset);
    } else if (BITS(inst, 24, 27) == 7 && BIT(inst, 21) == 0 && BITS(inst, 4, 11) == 0) {
        return LnSWoUB(RegisterOffset);
    } else if (BITS(inst, 24, 27) == 7 && BIT(inst, 21) == 0 && BIT(inst, 4) == 0) {
        return LnSWoUB(ScaledRegisterOffset);
    } else if (BITS(inst, 24, 27) == 5 && BIT(inst, 21) == 1) {
        return LnSWoUB(ImmediatePreIndexed);
    } else if (BITS(inst, 24, 27) == 7 && BIT(inst, 21) == 1 && BITS(inst, 4, 11) == 0) {
        return LnSWoUB(RegisterPreIndexed);
    } else if (BITS(inst, 24, 27) == 7 && BIT(inst, 21) == 1 && BIT(inst, 4) == 0) {
        return LnSWoUB(ScaledRegisterPreIndexed);
    } else if (BITS(inst, 24, 27) == 4 && BIT(inst, 21) == 0) {
        return LnSWoUB(ImmediatePostIndexed);
    } else if (BITS(inst, 24, 27) == 6 && BIT(inst, 21) == 0 && BITS(inst, 4, 11) == 0) {
        return LnSWoUB(RegisterPostIndexed);
    } else if (BITS(inst, 24, 27) == 6 && BIT(inst, 21) == 0 && BIT(inst, 4) == 0) {
        return LnSWoUB(ScaledRegisterPostIndexed);
    } else if (BITS(inst, 24, 27) == 1 && BITS(inst, 21, 22) == 2 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
        return MLnS(ImmediateOffset);
    } else if (BITS(inst, 24, 27) == 1 && BITS(inst, 21, 22) == 0 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
        return MLnS(RegisterOffset);
    } else if (BITS(inst, 24, 27) == 1 && BITS(inst, 21, 22) == 3 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
        return MLnS(ImmediatePreIndexed);
    } else if (BITS(inst, 24, 27) == 1 && BITS(inst, 21, 22) == 1 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
        return MLnS(RegisterPreIndexed);
    } else if (BITS(inst, 24, 27) == 0 && BITS(inst, 21, 22) == 2 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
        return MLnS(ImmediatePostIndexed);
    } else if (BITS(inst, 24, 27) == 0 && BITS(inst, 21, 22) == 0 && BIT(inst, 7) == 1 && BIT(inst, 4) == 1) {
        return MLnS(RegisterPostIndexed);
    } else if (BITS(inst, 23, 27) == 0x11) {
        return LdnStM(IncrementAfter);
    } else if (BITS(inst, 23, 27) == 0x13) {
        return LdnStM(IncrementBefore);
    } else if (BITS(inst, 23, 27) == 0x10) {
        return LdnStM(DecrementAfter);
    } else if (BITS(inst, 23, 27) == 0x12) {
        return LdnStM(DecrementBefore);
    }
    return nullptr;
}

// Specialized for LDRT, LDRBT, STRT, and STRBT, which have specific addressing mode requirements
get_addr_fp_t GetAddressingOpLoadStoreT(unsigned int inst) {
    if (BITS(inst, 25, 27) == 2) {
        return LnSWoUB(ImmediatePostIndexed);
    } else if (BITS(inst, 25, 27) == 3) {
        return LnSWoUB(ScaledRegisterPostIndexed);
    }
    // Reaching this would indicate the thumb version
    // of this instruction, however the 3DS CPU doesn't
    // support this variant (the 3DS CPU is only ARMv6K,
    // while this variant is added in ARMv6T2).
    // So it's sufficient for citra to not implement this.
    return nullptr;
}

enum { FETCH_SUCCESS,
    FETCH_FAILURE };

static ThumbDecodeStatus decode_thumb_instruction(ARMul_State *cpu, std::uint32_t inst, std::uint32_t addr, std::uint32_t *arm_inst, std::uint32_t *inst_size,
    ARM_INST_PTR *ptr_inst_base) {
    // Check if in Thumb mode
    ThumbDecodeStatus ret = TranslateThumbInstruction(addr, inst, arm_inst, inst_size);
    if (ret == ThumbDecodeStatus::BRANCH) {
        int inst_index;
        int table_length = static_cast<int>(arm_instruction_trans_len);
        std::uint32_t tinstr = GetThumbInstruction(inst, addr);

        switch ((tinstr & 0xF800) >> 11) {
        case 26:
        case 27:
            if (((tinstr & 0x0F00) != 0x0E00) && ((tinstr & 0x0F00) != 0x0F00)) {
                inst_index = table_length - 4;
                *ptr_inst_base = arm_instruction_trans[inst_index](cpu, tinstr, inst_index);
            } else {
                LOG_ERROR(eka2l1::CPU_DYNCOM, "thumb decoder error");
            }
            break;
        case 28:
            // Branch 2, unconditional branch
            inst_index = table_length - 5;
            *ptr_inst_base = arm_instruction_trans[inst_index](cpu, tinstr, inst_index);
            break;

        case 8:
        case 29:
            // For BLX 1 thumb instruction
            inst_index = table_length - 1;
            *ptr_inst_base = arm_instruction_trans[inst_index](cpu, tinstr, inst_index);
            break;
        case 30:
            // For BL 1 thumb instruction
            inst_index = table_length - 3;
            *ptr_inst_base = arm_instruction_trans[inst_index](cpu, tinstr, inst_index);
            break;
        case 31:
            // For BL 2 thumb instruction
            inst_index = table_length - 2;
            *ptr_inst_base = arm_instruction_trans[inst_index](cpu, tinstr, inst_index);
            break;
        default:
            ret = ThumbDecodeStatus::UNDEFINED;
            break;
        }
    }
    return ret;
}

enum { KEEP_GOING,
    FETCH_EXCEPTION };

static unsigned int InterpreterTranslateInstruction(ARMul_State *cpu, const std::uint32_t phys_addr,
    ARM_INST_PTR &inst_base) {
    std::uint32_t inst_size = 4;
    std::uint32_t inst = cpu->ReadCode(phys_addr & 0xFFFFFFFC);

    // If we are in Thumb mode, we'll translate one Thumb instruction to the corresponding ARM
    // instruction
    if (cpu->TFlag) {
        std::uint32_t arm_inst;
        ThumbDecodeStatus state = decode_thumb_instruction(cpu, inst, phys_addr, &arm_inst, &inst_size, &inst_base);

        // We have translated the Thumb branch instruction in the Thumb decoder
        if (state == ThumbDecodeStatus::BRANCH) {
            return inst_size;
        }
        inst = arm_inst;
    }

    int idx;
    if (decode_arm_instruction(inst, &idx) == ARMDecodeStatus::FAILURE) {
        LOG_ERROR(eka2l1::CPU_DYNCOM, "Decode failure.\tPC: [{:#010X}]\tInstruction: {:08X}", phys_addr,
            inst);
        LOG_ERROR(eka2l1::CPU_DYNCOM, "cpsr={:#X}, cpu->TFlag={}, r15={:#010X}", cpu->Cpsr, cpu->TFlag,
            cpu->Reg[15]);
        cpu->RaiseException(eka2l1::arm::exception_type_undefined_inst, phys_addr);
        inst_base = nullptr;
        return 0;
    }
    inst_base = arm_instruction_trans[idx](cpu, inst, idx);

    return inst_size;
}

static int InterpreterTranslateBlock(ARMul_State *cpu, std::size_t &bb_start, std::uint32_t addr) {
    // Decode instruction, get index
    // Allocate memory and init InsCream
    // Go on next, until terminal instruction
    // Save start addr of basicblock in CreamCache
    ARM_INST_PTR inst_base = nullptr;
    TransExtData ret = TransExtData::NON_BRANCH;
    int size = 0; // instruction size of basic block
    bb_start = cpu->trans_cache_buf_top;

    std::uint32_t phys_addr = addr;
    std::uint32_t pc_start = cpu->Reg[15];

    // Bulk-loop acceleration pre-pass: if this Thumb block is a canonical
    // copy/convert/fill do-while loop, prepend a synthetic instruction that
    // batches iterations natively (see loop_accel_inst above). The block body
    // itself is still translated normally right after it.
    if (cpu->TFlag) {
        loop_accel_inst accel_desc;
        if (analyze_thumb_bulk_loop(cpu, pc_start, accel_desc)) {
            const std::size_t alloc_size = sizeof(arm_inst) + sizeof(loop_accel_inst);
            arm_inst *accel_base = reinterpret_cast<arm_inst *>(&cpu->trans_cache_buf[cpu->trans_cache_buf_top]);
            cpu->trans_cache_buf_top += ((alloc_size + 7) >> 3) << 3;
            accel_base->idx = LOOP_ACCEL_IDX;
            accel_base->cond = ConditionCode::AL;
            accel_base->br = TransExtData::NON_BRANCH;
            *reinterpret_cast<loop_accel_inst *>(accel_base->component) = accel_desc;
#if defined(EKA2L1_DYNCOM_DIFFTEST)
            ++g_loop_accel_attaches;
#endif
        }
    }

    while (ret == TransExtData::NON_BRANCH) {
        unsigned int inst_size = InterpreterTranslateInstruction(cpu, phys_addr, inst_base);

        if (!inst_base) {
            return FETCH_EXCEPTION;
        }

        size++;

        phys_addr += inst_size;

        if ((phys_addr & 0xfff) == 0) {
            inst_base->br = TransExtData::END_OF_PAGE;
        }
        ret = inst_base->br;
    };

    cpu->instruction_cache[cpu->make_instruction_cache_key(pc_start)] = bb_start;

    return KEEP_GOING;
}

static int InterpreterTranslateSingle(ARMul_State *cpu, std::size_t &bb_start, std::uint32_t addr) {
    ARM_INST_PTR inst_base = nullptr;
    bb_start = cpu->trans_cache_buf_top;

    std::uint32_t phys_addr = addr;
    std::uint32_t pc_start = cpu->Reg[15];

    InterpreterTranslateInstruction(cpu, phys_addr, inst_base);

    if (!inst_base) {
        return FETCH_EXCEPTION;
    }

    if (inst_base->br == TransExtData::NON_BRANCH) {
        inst_base->br = TransExtData::SINGLE_STEP;
    }

    cpu->instruction_cache[cpu->make_instruction_cache_key(pc_start)] = bb_start;

    return KEEP_GOING;
}

static int clz(unsigned int x) {
    int n;
    if (x == 0)
        return (32);
    n = 1;
    if ((x >> 16) == 0) {
        n = n + 16;
        x = x << 16;
    }
    if ((x >> 24) == 0) {
        n = n + 8;
        x = x << 8;
    }
    if ((x >> 28) == 0) {
        n = n + 4;
        x = x << 4;
    }
    if ((x >> 30) == 0) {
        n = n + 2;
        x = x << 2;
    }
    n = n - (x >> 31);
    return n;
}

unsigned InterpreterMainLoop(ARMul_State *cpu, std::uint32_t &num_instrs) {
#undef RM
#undef RS

#define CRn inst_cream->crn
#define OPCODE_1 inst_cream->opcode_1
#define OPCODE_2 inst_cream->opcode_2
#define CRm inst_cream->crm
#define RD cpu->Reg[inst_cream->Rd]
#define RD2 cpu->Reg[inst_cream->Rd + 1]
#define RN cpu->Reg[inst_cream->Rn]
#define RM cpu->Reg[inst_cream->Rm]
#define RS cpu->Reg[inst_cream->Rs]
#define RDHI cpu->Reg[inst_cream->RdHi]
#define RDLO cpu->Reg[inst_cream->RdLo]
#define LINK_RTN_ADDR (cpu->Reg[14] = cpu->Reg[15] + 4)
#define SET_PC (cpu->Reg[15] = cpu->Reg[15] + 8 + inst_cream->signed_immed_24)
// Like LS_GET_ADDR, but for the data-processing shifter operand: inline the
// dominant forms at the call site behind well-predicted pointer compares,
// instead of the polymorphic indirect shtop_func() call. Immediate (`#imm` ->
// rotate of imm8), plain register (`Rm`, no shift -- the dominant form in
// Thumb-translated code) and the immediate LSL/ASR/LSR shifts are handled
// inline; register-specified shifts and rotates fall back. A
// statement-expression keeps it usable wherever the old macro was an
// expression.
#define SHIFTER_OPERAND compute_shifter_operand(cpu, inst_cream->shtop_func, inst_cream->shifter_operand)

#define FETCH_INST                                 \
    if (inst_base->br != TransExtData::NON_BRANCH) \
        goto DISPATCH;                             \
    inst_base = (arm_inst *)&cpu->trans_cache_buf[ptr]

#define INC_PC(l) ptr += (((sizeof(arm_inst) + l + 7) >> 3) << 3)
#define INC_PC_STUB ptr += (((sizeof(arm_inst) + 7) >> 3) << 3)

// GCC and Clang have a C++ extension to support a lookup table of labels. Otherwise, fallback to a
// clunky switch statement.
#if defined __GNUC__ || defined __clang__
#define GOTO_NEXT_INST                         \
    PROF_STEP(cpu, inst_base->idx);            \
    if (num_instrs >= cpu->NumInstrsToExecute) \
        goto END;                              \
    num_instrs++;                              \
    goto *InstLabel[inst_base->idx]
#else
#define GOTO_NEXT_INST                         \
    PROF_STEP(cpu, inst_base->idx);            \
    if (num_instrs >= cpu->NumInstrsToExecute) \
        goto END;                              \
    num_instrs++;                              \
    switch (inst_base->idx) {                  \
    case 0:                                    \
        goto VMLA_INST;                        \
    case 1:                                    \
        goto VMLS_INST;                        \
    case 2:                                    \
        goto VNMLA_INST;                       \
    case 3:                                    \
        goto VNMLS_INST;                       \
    case 4:                                    \
        goto VNMUL_INST;                       \
    case 5:                                    \
        goto VMUL_INST;                        \
    case 6:                                    \
        goto VADD_INST;                        \
    case 7:                                    \
        goto VSUB_INST;                        \
    case 8:                                    \
        goto VDIV_INST;                        \
    case 9:                                    \
        goto VMOVI_INST;                       \
    case 10:                                   \
        goto VMOVR_INST;                       \
    case 11:                                   \
        goto VABS_INST;                        \
    case 12:                                   \
        goto VNEG_INST;                        \
    case 13:                                   \
        goto VSQRT_INST;                       \
    case 14:                                   \
        goto VCMP_INST;                        \
    case 15:                                   \
        goto VCMP2_INST;                       \
    case 16:                                   \
        goto VCVTBDS_INST;                     \
    case 17:                                   \
        goto VCVTBFF_INST;                     \
    case 18:                                   \
        goto VCVTBFI_INST;                     \
    case 19:                                   \
        goto VMOVBRS_INST;                     \
    case 20:                                   \
        goto VMSR_INST;                        \
    case 21:                                   \
        goto VMOVBRC_INST;                     \
    case 22:                                   \
        goto VMRS_INST;                        \
    case 23:                                   \
        goto VMOVBCR_INST;                     \
    case 24:                                   \
        goto VMOVBRRSS_INST;                   \
    case 25:                                   \
        goto VMOVBRRD_INST;                    \
    case 26:                                   \
        goto VSTR_INST;                        \
    case 27:                                   \
        goto VPUSH_INST;                       \
    case 28:                                   \
        goto VSTM_INST;                        \
    case 29:                                   \
        goto VPOP_INST;                        \
    case 30:                                   \
        goto VLDR_INST;                        \
    case 31:                                   \
        goto VLDM_INST;                        \
    case 32:                                   \
        goto SRS_INST;                         \
    case 33:                                   \
        goto RFE_INST;                         \
    case 34:                                   \
        goto BKPT_INST;                        \
    case 35:                                   \
        goto BLX_INST;                         \
    case 36:                                   \
        goto CPS_INST;                         \
    case 37:                                   \
        goto PLD_INST;                         \
    case 38:                                   \
        goto SETEND_INST;                      \
    case 39:                                   \
        goto CLREX_INST;                       \
    case 40:                                   \
        goto REV16_INST;                       \
    case 41:                                   \
        goto USAD8_INST;                       \
    case 42:                                   \
        goto SXTB_INST;                        \
    case 43:                                   \
        goto UXTB_INST;                        \
    case 44:                                   \
        goto SXTH_INST;                        \
    case 45:                                   \
        goto SXTB16_INST;                      \
    case 46:                                   \
        goto UXTH_INST;                        \
    case 47:                                   \
        goto UXTB16_INST;                      \
    case 48:                                   \
        goto CPY_INST;                         \
    case 49:                                   \
        goto UXTAB_INST;                       \
    case 50:                                   \
        goto SSUB8_INST;                       \
    case 51:                                   \
        goto SHSUB8_INST;                      \
    case 52:                                   \
        goto SSUBADDX_INST;                    \
    case 53:                                   \
        goto STREX_INST;                       \
    case 54:                                   \
        goto STREXB_INST;                      \
    case 55:                                   \
        goto SWP_INST;                         \
    case 56:                                   \
        goto SWPB_INST;                        \
    case 57:                                   \
        goto SSUB16_INST;                      \
    case 58:                                   \
        goto SSAT16_INST;                      \
    case 59:                                   \
        goto SHSUBADDX_INST;                   \
    case 60:                                   \
        goto QSUBADDX_INST;                    \
    case 61:                                   \
        goto SHADDSUBX_INST;                   \
    case 62:                                   \
        goto SHADD8_INST;                      \
    case 63:                                   \
        goto SHADD16_INST;                     \
    case 64:                                   \
        goto SEL_INST;                         \
    case 65:                                   \
        goto SADDSUBX_INST;                    \
    case 66:                                   \
        goto SADD8_INST;                       \
    case 67:                                   \
        goto SADD16_INST;                      \
    case 68:                                   \
        goto SHSUB16_INST;                     \
    case 69:                                   \
        goto UMAAL_INST;                       \
    case 70:                                   \
        goto UXTAB16_INST;                     \
    case 71:                                   \
        goto USUBADDX_INST;                    \
    case 72:                                   \
        goto USUB8_INST;                       \
    case 73:                                   \
        goto USUB16_INST;                      \
    case 74:                                   \
        goto USAT16_INST;                      \
    case 75:                                   \
        goto USADA8_INST;                      \
    case 76:                                   \
        goto UQSUBADDX_INST;                   \
    case 77:                                   \
        goto UQSUB8_INST;                      \
    case 78:                                   \
        goto UQSUB16_INST;                     \
    case 79:                                   \
        goto UQADDSUBX_INST;                   \
    case 80:                                   \
        goto UQADD8_INST;                      \
    case 81:                                   \
        goto UQADD16_INST;                     \
    case 82:                                   \
        goto SXTAB_INST;                       \
    case 83:                                   \
        goto UHSUBADDX_INST;                   \
    case 84:                                   \
        goto UHSUB8_INST;                      \
    case 85:                                   \
        goto UHSUB16_INST;                     \
    case 86:                                   \
        goto UHADDSUBX_INST;                   \
    case 87:                                   \
        goto UHADD8_INST;                      \
    case 88:                                   \
        goto UHADD16_INST;                     \
    case 89:                                   \
        goto UADDSUBX_INST;                    \
    case 90:                                   \
        goto UADD8_INST;                       \
    case 91:                                   \
        goto UADD16_INST;                      \
    case 92:                                   \
        goto SXTAH_INST;                       \
    case 93:                                   \
        goto SXTAB16_INST;                     \
    case 94:                                   \
        goto QADD8_INST;                       \
    case 95:                                   \
        goto BXJ_INST;                         \
    case 96:                                   \
        goto CLZ_INST;                         \
    case 97:                                   \
        goto UXTAH_INST;                       \
    case 98:                                   \
        goto BX_INST;                          \
    case 99:                                   \
        goto REV_INST;                         \
    case 100:                                  \
        goto BLX_INST;                         \
    case 101:                                  \
        goto REVSH_INST;                       \
    case 102:                                  \
        goto QADD_INST;                        \
    case 103:                                  \
        goto QADD16_INST;                      \
    case 104:                                  \
        goto QADDSUBX_INST;                    \
    case 105:                                  \
        goto LDREX_INST;                       \
    case 106:                                  \
        goto QDADD_INST;                       \
    case 107:                                  \
        goto QDSUB_INST;                       \
    case 108:                                  \
        goto QSUB_INST;                        \
    case 109:                                  \
        goto LDREXB_INST;                      \
    case 110:                                  \
        goto QSUB8_INST;                       \
    case 111:                                  \
        goto QSUB16_INST;                      \
    case 112:                                  \
        goto SMUAD_INST;                       \
    case 113:                                  \
        goto SMMUL_INST;                       \
    case 114:                                  \
        goto SMUSD_INST;                       \
    case 115:                                  \
        goto SMLSD_INST;                       \
    case 116:                                  \
        goto SMLSLD_INST;                      \
    case 117:                                  \
        goto SMMLA_INST;                       \
    case 118:                                  \
        goto SMMLS_INST;                       \
    case 119:                                  \
        goto SMLALD_INST;                      \
    case 120:                                  \
        goto SMLAD_INST;                       \
    case 121:                                  \
        goto SMLAW_INST;                       \
    case 122:                                  \
        goto SMULW_INST;                       \
    case 123:                                  \
        goto PKHTB_INST;                       \
    case 124:                                  \
        goto PKHBT_INST;                       \
    case 125:                                  \
        goto SMUL_INST;                        \
    case 126:                                  \
        goto SMLALXY_INST;                     \
    case 127:                                  \
        goto SMLA_INST;                        \
    case 128:                                  \
        goto MCRR_INST;                        \
    case 129:                                  \
        goto MRRC_INST;                        \
    case 130:                                  \
        goto CMP_INST;                         \
    case 131:                                  \
        goto TST_INST;                         \
    case 132:                                  \
        goto TEQ_INST;                         \
    case 133:                                  \
        goto CMN_INST;                         \
    case 134:                                  \
        goto SMULL_INST;                       \
    case 135:                                  \
        goto UMULL_INST;                       \
    case 136:                                  \
        goto UMLAL_INST;                       \
    case 137:                                  \
        goto SMLAL_INST;                       \
    case 138:                                  \
        goto MUL_INST;                         \
    case 139:                                  \
        goto MLA_INST;                         \
    case 140:                                  \
        goto SSAT_INST;                        \
    case 141:                                  \
        goto USAT_INST;                        \
    case 142:                                  \
        goto MRS_INST;                         \
    case 143:                                  \
        goto MSR_INST;                         \
    case 144:                                  \
        goto AND_INST;                         \
    case 145:                                  \
        goto BIC_INST;                         \
    case 146:                                  \
        goto LDM_INST;                         \
    case 147:                                  \
        goto EOR_INST;                         \
    case 148:                                  \
        goto ADD_INST;                         \
    case 149:                                  \
        goto RSB_INST;                         \
    case 150:                                  \
        goto RSC_INST;                         \
    case 151:                                  \
        goto SBC_INST;                         \
    case 152:                                  \
        goto ADC_INST;                         \
    case 153:                                  \
        goto SUB_INST;                         \
    case 154:                                  \
        goto ORR_INST;                         \
    case 155:                                  \
        goto MVN_INST;                         \
    case 156:                                  \
        goto MOV_INST;                         \
    case 157:                                  \
        goto STM_INST;                         \
    case 158:                                  \
        goto LDM_INST;                         \
    case 159:                                  \
        goto LDRSH_INST;                       \
    case 160:                                  \
        goto STM_INST;                         \
    case 161:                                  \
        goto LDM_INST;                         \
    case 162:                                  \
        goto LDRSB_INST;                       \
    case 163:                                  \
        goto STRD_INST;                        \
    case 164:                                  \
        goto LDRH_INST;                        \
    case 165:                                  \
        goto STRH_INST;                        \
    case 166:                                  \
        goto LDRD_INST;                        \
    case 167:                                  \
        goto STRT_INST;                        \
    case 168:                                  \
        goto STRBT_INST;                       \
    case 169:                                  \
        goto LDRBT_INST;                       \
    case 170:                                  \
        goto LDRT_INST;                        \
    case 171:                                  \
        goto MRC_INST;                         \
    case 172:                                  \
        goto MCR_INST;                         \
    case 173:                                  \
        goto MSR_INST;                         \
    case 174:                                  \
        goto MSR_INST;                         \
    case 175:                                  \
        goto MSR_INST;                         \
    case 176:                                  \
        goto MSR_INST;                         \
    case 177:                                  \
        goto MSR_INST;                         \
    case 178:                                  \
        goto LDRB_INST;                        \
    case 179:                                  \
        goto STRB_INST;                        \
    case 180:                                  \
        goto LDR_INST;                         \
    case 181:                                  \
        goto LDRCOND_INST;                     \
    case 182:                                  \
        goto STR_INST;                         \
    case 183:                                  \
        goto CDP_INST;                         \
    case 184:                                  \
        goto STC_INST;                         \
    case 185:                                  \
        goto LDC_INST;                         \
    case 186:                                  \
        goto LDREXD_INST;                      \
    case 187:                                  \
        goto STREXD_INST;                      \
    case 188:                                  \
        goto LDREXH_INST;                      \
    case 189:                                  \
        goto STREXH_INST;                      \
    case 190:                                  \
        goto NOP_INST;                         \
    case 191:                                  \
        goto YIELD_INST;                       \
    case 192:                                  \
        goto WFE_INST;                         \
    case 193:                                  \
        goto WFI_INST;                         \
    case 194:                                  \
        goto SEV_INST;                         \
    case 195:                                  \
        goto SWI_INST;                         \
    case 196:                                  \
        goto BBL_INST;                         \
    case 197:                                  \
        goto B_2_THUMB;                        \
    case 198:                                  \
        goto B_COND_THUMB;                     \
    case 199:                                  \
        goto BL_1_THUMB;                       \
    case 200:                                  \
        goto BL_2_THUMB;                       \
    case 201:                                  \
        goto BLX_1_THUMB;                      \
    case 202:                                  \
        goto DISPATCH;                         \
    case 203:                                  \
        goto INIT_INST_LENGTH;                 \
    case 204:                                  \
        goto END;                              \
    case 205:                                  \
        goto LOOP_ACCEL_INST;                  \
    }
#endif

#define UPDATE_NFLAG(dst) (cpu->NFlag = BIT(dst, 31) ? 1 : 0)
#define UPDATE_ZFLAG(dst) (cpu->ZFlag = dst ? 0 : 1)
#define UPDATE_CFLAG_WITH_SC (cpu->CFlag = cpu->shifter_carry_out)

#define SAVE_NZCVT \
    cpu->Cpsr = (cpu->Cpsr & 0x0fffffdf) | (cpu->NFlag << 31) | (cpu->ZFlag << 30) | (cpu->CFlag << 29) | (cpu->VFlag << 28) | (cpu->TFlag << 5)
#define LOAD_NZCVT                      \
    cpu->NFlag = (cpu->Cpsr >> 31);     \
    cpu->ZFlag = (cpu->Cpsr >> 30) & 1; \
    cpu->CFlag = (cpu->Cpsr >> 29) & 1; \
    cpu->VFlag = (cpu->Cpsr >> 28) & 1; \
    cpu->TFlag = (cpu->Cpsr >> 5) & 1;

#define PC (cpu->Reg[15])

// GCC and Clang have a C++ extension to support a lookup table of labels. Otherwise, fallback
// to a clunky switch statement.
#if defined __GNUC__ || defined __clang__
    void *InstLabel[] = { &&VMLA_INST,
        &&VMLS_INST,
        &&VNMLA_INST,
        &&VNMLS_INST,
        &&VNMUL_INST,
        &&VMUL_INST,
        &&VADD_INST,
        &&VSUB_INST,
        &&VDIV_INST,
        &&VMOVI_INST,
        &&VMOVR_INST,
        &&VABS_INST,
        &&VNEG_INST,
        &&VSQRT_INST,
        &&VCMP_INST,
        &&VCMP2_INST,
        &&VCVTBDS_INST,
        &&VCVTBFF_INST,
        &&VCVTBFI_INST,
        &&VMOVBRS_INST,
        &&VMSR_INST,
        &&VMOVBRC_INST,
        &&VMRS_INST,
        &&VMOVBCR_INST,
        &&VMOVBRRSS_INST,
        &&VMOVBRRD_INST,
        &&VSTR_INST,
        &&VPUSH_INST,
        &&VSTM_INST,
        &&VPOP_INST,
        &&VLDR_INST,
        &&VLDM_INST,

        &&SRS_INST,
        &&RFE_INST,
        &&BKPT_INST,
        &&BLX_INST,
        &&CPS_INST,
        &&PLD_INST,
        &&SETEND_INST,
        &&CLREX_INST,
        &&REV16_INST,
        &&USAD8_INST,
        &&SXTB_INST,
        &&UXTB_INST,
        &&SXTH_INST,
        &&SXTB16_INST,
        &&UXTH_INST,
        &&UXTB16_INST,
        &&CPY_INST,
        &&UXTAB_INST,
        &&SSUB8_INST,
        &&SHSUB8_INST,
        &&SSUBADDX_INST,
        &&STREX_INST,
        &&STREXB_INST,
        &&SWP_INST,
        &&SWPB_INST,
        &&SSUB16_INST,
        &&SSAT16_INST,
        &&SHSUBADDX_INST,
        &&QSUBADDX_INST,
        &&SHADDSUBX_INST,
        &&SHADD8_INST,
        &&SHADD16_INST,
        &&SEL_INST,
        &&SADDSUBX_INST,
        &&SADD8_INST,
        &&SADD16_INST,
        &&SHSUB16_INST,
        &&UMAAL_INST,
        &&UXTAB16_INST,
        &&USUBADDX_INST,
        &&USUB8_INST,
        &&USUB16_INST,
        &&USAT16_INST,
        &&USADA8_INST,
        &&UQSUBADDX_INST,
        &&UQSUB8_INST,
        &&UQSUB16_INST,
        &&UQADDSUBX_INST,
        &&UQADD8_INST,
        &&UQADD16_INST,
        &&SXTAB_INST,
        &&UHSUBADDX_INST,
        &&UHSUB8_INST,
        &&UHSUB16_INST,
        &&UHADDSUBX_INST,
        &&UHADD8_INST,
        &&UHADD16_INST,
        &&UADDSUBX_INST,
        &&UADD8_INST,
        &&UADD16_INST,
        &&SXTAH_INST,
        &&SXTAB16_INST,
        &&QADD8_INST,
        &&BXJ_INST,
        &&CLZ_INST,
        &&UXTAH_INST,
        &&BX_INST,
        &&REV_INST,
        &&BLX_INST,
        &&REVSH_INST,
        &&QADD_INST,
        &&QADD16_INST,
        &&QADDSUBX_INST,
        &&LDREX_INST,
        &&QDADD_INST,
        &&QDSUB_INST,
        &&QSUB_INST,
        &&LDREXB_INST,
        &&QSUB8_INST,
        &&QSUB16_INST,
        &&SMUAD_INST,
        &&SMMUL_INST,
        &&SMUSD_INST,
        &&SMLSD_INST,
        &&SMLSLD_INST,
        &&SMMLA_INST,
        &&SMMLS_INST,
        &&SMLALD_INST,
        &&SMLAD_INST,
        &&SMLAW_INST,
        &&SMULW_INST,
        &&PKHTB_INST,
        &&PKHBT_INST,
        &&SMUL_INST,
        &&SMLALXY_INST,
        &&SMLA_INST,
        &&MCRR_INST,
        &&MRRC_INST,
        &&CMP_INST,
        &&TST_INST,
        &&TEQ_INST,
        &&CMN_INST,
        &&SMULL_INST,
        &&UMULL_INST,
        &&UMLAL_INST,
        &&SMLAL_INST,
        &&MUL_INST,
        &&MLA_INST,
        &&SSAT_INST,
        &&USAT_INST,
        &&MRS_INST,
        &&MSR_INST,
        &&AND_INST,
        &&BIC_INST,
        &&LDM_INST,
        &&EOR_INST,
        &&ADD_INST,
        &&RSB_INST,
        &&RSC_INST,
        &&SBC_INST,
        &&ADC_INST,
        &&SUB_INST,
        &&ORR_INST,
        &&MVN_INST,
        &&MOV_INST,
        &&STM_INST,
        &&LDM_INST,
        &&LDRSH_INST,
        &&STM_INST,
        &&LDM_INST,
        &&LDRSB_INST,
        &&STRD_INST,
        &&LDRH_INST,
        &&STRH_INST,
        &&LDRD_INST,
        &&STRT_INST,
        &&STRBT_INST,
        &&LDRBT_INST,
        &&LDRT_INST,
        &&MRC_INST,
        &&MCR_INST,
        &&MSR_INST,
        &&MSR_INST,
        &&MSR_INST,
        &&MSR_INST,
        &&MSR_INST,
        &&LDRB_INST,
        &&STRB_INST,
        &&LDR_INST,
        &&LDRCOND_INST,
        &&STR_INST,
        &&CDP_INST,
        &&STC_INST,
        &&LDC_INST,
        &&LDREXD_INST,
        &&STREXD_INST,
        &&LDREXH_INST,
        &&STREXH_INST,
        &&NOP_INST,
        &&YIELD_INST,
        &&WFE_INST,
        &&WFI_INST,
        &&SEV_INST,
        &&SWI_INST,
        &&BBL_INST,
        &&B_2_THUMB,
        &&B_COND_THUMB,
        &&BL_1_THUMB,
        &&BL_2_THUMB,
        &&BLX_1_THUMB,
        &&DISPATCH,
        &&INIT_INST_LENGTH,
        &&END,
        &&LOOP_ACCEL_INST };
#endif
    arm_inst *inst_base;
    unsigned int addr;
    unsigned int undef_inst = 0;

    std::size_t ptr;

    LOAD_NZCVT;
DISPATCH : {
    PROF_BLOCK_ENTER(cpu);
    if (!cpu->NirqSig) {
        if (!(cpu->Cpsr & 0x80)) {
            goto END;
        }
    }

    if (cpu->TFlag)
        cpu->Reg[15] &= 0xfffffffe;
    else
        cpu->Reg[15] &= 0xfffffffc;

    // Find the cached instruction cream, otherwise translate it...
    const std::uint64_t block_key = cpu->make_instruction_cache_key(cpu->Reg[15]);
    ARMul_State::block_l1_entry &l1 = cpu->block_l1_cache[ARMul_State::block_l1_index(block_key)];
    if (l1.key == block_key) {
        // Fast path: hot blocks skip the unordered_map entirely.
        ptr = l1.ptr;
    } else {
        auto itr = cpu->instruction_cache.find(block_key);
        if (itr != cpu->instruction_cache.end()) {
            ptr = itr->second;
        } else {
            // The translation buffer is a bump allocator that is no longer reset
            // on every context switch (blocks are kept across processes via the
            // asid tag). Flush everything if a fresh block could run past the
            // buffer end. TRANS_CACHE_FLUSH_RESERVE comfortably exceeds the
            // largest possible single basic block (capped at one page).
            constexpr std::size_t TRANS_CACHE_FLUSH_RESERVE = 2 * 1024 * 1024;
            if (cpu->trans_cache_buf_top + TRANS_CACHE_FLUSH_RESERVE > TRANS_CACHE_SIZE) {
                cpu->instruction_cache.clear();
                cpu->trans_cache_buf_top = 0;
                cpu->flush_block_l1_cache();
            }

            if (cpu->NumInstrsToExecute != 1) {
                if (InterpreterTranslateBlock(cpu, ptr, cpu->Reg[15]) == FETCH_EXCEPTION)
                    goto END;
            } else {
                if (InterpreterTranslateSingle(cpu, ptr, cpu->Reg[15]) == FETCH_EXCEPTION)
                    goto END;
            }
        }

        // Re-index: a flush above may have moved the slot; refill from the live
        // key so the next visit hits. (l1 reference is still valid -- the cache
        // is a fixed array -- but recompute defensively after a possible flush.)
        ARMul_State::block_l1_entry &slot = cpu->block_l1_cache[ARMul_State::block_l1_index(block_key)];
        slot.key = block_key;
        slot.ptr = ptr;
    }

    inst_base = (arm_inst *)&cpu->trans_cache_buf[ptr];
    GOTO_NEXT_INST;
}
LOOP_ACCEL_INST : {
    loop_accel_inst *const inst_cream = (loop_accel_inst *)inst_base->component;
    const std::uint32_t iter_left = cpu->Reg[inst_cream->counter_reg];
    if (iter_left > 1) {
        // Bulk at most iter_left-1 iterations: the last one always runs
        // interpreted so flags, scratch registers and the loop exit come from
        // real interpretation. Cap by the remaining quantum so blocking /
        // reschedule behaviour keeps its granularity.
        const std::uint64_t quantum_rem = (cpu->NumInstrsToExecute > num_instrs)
            ? (cpu->NumInstrsToExecute - num_instrs)
            : 0;
        const std::uint64_t cap = quantum_rem / inst_cream->body_len + 1;
        const std::uint32_t want = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(cap, iter_left - 1));
        if (want) {
            const std::uint32_t done = run_accel_bulk(cpu, inst_cream, want);
            if (done) {
                for (int i = 0; i < inst_cream->ind_count; i++)
                    cpu->Reg[inst_cream->ind[i].reg] += static_cast<std::uint32_t>(inst_cream->ind[i].delta) * done;
                num_instrs += static_cast<std::uint64_t>(done) * inst_cream->body_len;
            }
        }
    }
    INC_PC(sizeof(loop_accel_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
ADC_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        adc_inst *const inst_cream = (adc_inst *)inst_base->component;

        std::uint32_t rn_val = RN;
        if (inst_cream->Rn == 15)
            rn_val += 2 * cpu->GetInstructionSize();

        bool carry;
        bool overflow;
        RD = AddWithCarry(rn_val, SHIFTER_OPERAND, cpu->CFlag, &carry, &overflow);

        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            cpu->CFlag = carry;
            cpu->VFlag = overflow;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(adc_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(adc_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
ADD_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        add_inst *const inst_cream = (add_inst *)inst_base->component;
        std::uint32_t rn_val = 0;

        // The ADR thumb instruction got disguised, under ADD. However unlike the other,
        // it uses aligned PC. So have to check -- but the distinction only matters when
        // Rn is the PC, so ordinary registers skip the per-execution code re-read.
        if (inst_cream->Rn != 15) {
            rn_val = cpu->Reg[inst_cream->Rn];
        } else if (cpu->TFlag) {
            std::uint32_t inst = cpu->ReadCode(cpu->Reg[15] & 0xFFFFFFFC);
            inst = GetThumbInstruction(inst, cpu->Reg[15]);

            if (((inst & 0xF800) >> 11) == 20) {
                rn_val = CHECK_READ_REG15_WA(cpu, inst_cream->Rn);
            } else {
                rn_val = CHECK_READ_REG15(cpu, inst_cream->Rn);
            }
        } else {
            rn_val = CHECK_READ_REG15_WA(cpu, inst_cream->Rn);
        }

        bool carry;
        bool overflow;
        RD = AddWithCarry(rn_val, SHIFTER_OPERAND, 0, &carry, &overflow);

        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            cpu->CFlag = carry;
            cpu->VFlag = overflow;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(add_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(add_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
AND_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        and_inst *const inst_cream = (and_inst *)inst_base->component;

        std::uint32_t lop = RN;
        std::uint32_t rop = SHIFTER_OPERAND;

        if (inst_cream->Rn == 15)
            lop += 2 * cpu->GetInstructionSize();

        RD = lop & rop;

        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Cpsr & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            UPDATE_CFLAG_WITH_SC;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(and_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(and_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
BBL_INST : {
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        bbl_inst *inst_cream = (bbl_inst *)inst_base->component;
        if (inst_cream->L) {
            LINK_RTN_ADDR;
        }
        SET_PC;
        INC_PC(sizeof(bbl_inst));
        goto DISPATCH;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(bbl_inst));
    goto DISPATCH;
}
BIC_INST : {
    bic_inst *inst_cream = (bic_inst *)inst_base->component;
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        std::uint32_t lop = RN;
        if (inst_cream->Rn == 15) {
            lop += 2 * cpu->GetInstructionSize();
        }
        std::uint32_t rop = SHIFTER_OPERAND;
        RD = lop & (~rop);
        if ((inst_cream->S) && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            UPDATE_CFLAG_WITH_SC;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(bic_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(bic_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
BKPT_INST : {
    const std::uint32_t pc = cpu->Reg[15];

    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        bkpt_inst *const inst_cream = (bkpt_inst *)inst_base->component;
        LOG_DEBUG(eka2l1::CPU_DYNCOM, "Breakpoint instruction hit. Immediate: {:#010X}", inst_cream->imm);

        // Call the handler
        SAVE_NZCVT;
        cpu->RaiseException(eka2l1::arm::exception_type_breakpoint, cpu->Reg[15]);

        LOAD_NZCVT;

        // A debugger or scripting hook may stop the core so it can restore and
        // single-step the displaced instruction. In that case the breakpoint
        // itself must not advance PC first.
        if (cpu->NumInstrsToExecute == 0) {
            goto END;
        }

        if (cpu->Reg[15] != pc) {
            goto DISPATCH;
        }
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(bkpt_inst));

    FETCH_INST;
    GOTO_NEXT_INST;
}
BLX_INST : {
    blx_inst *inst_cream = (blx_inst *)inst_base->component;
    if ((inst_base->cond == ConditionCode::AL) || CondPassed(cpu, inst_base->cond)) {
        unsigned int inst = inst_cream->inst;
        if (BITS(inst, 20, 27) == 0x12 && BITS(inst, 4, 7) == 0x3) {
            const std::uint32_t jump_address = cpu->Reg[inst_cream->val.Rm];
            cpu->Reg[14] = (cpu->Reg[15] + cpu->GetInstructionSize());
            if (cpu->TFlag)
                cpu->Reg[14] |= 0x1;
            cpu->Reg[15] = jump_address & 0xfffffffe;
            cpu->TFlag = jump_address & 0x1;
        } else {
            cpu->Reg[14] = (cpu->Reg[15] + cpu->GetInstructionSize());
            cpu->TFlag = 0x1;
            int signed_int = inst_cream->val.signed_immed_24;
            signed_int = (signed_int & 0x800000) ? (0x3F000000 | signed_int) : signed_int;
            signed_int = signed_int << 2;
            cpu->Reg[15] = cpu->Reg[15] + 8 + signed_int + (BIT(inst, 24) << 1);
        }
        INC_PC(sizeof(blx_inst));
        goto DISPATCH;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(blx_inst));
    goto DISPATCH;
}

BX_INST:
BXJ_INST : {
    // Note that only the 'fail' case of BXJ is emulated. This is because
    // the facilities for Jazelle emulation are not implemented.
    //
    // According to the ARM documentation on BXJ, if setting the J bit in the APSR
    // fails, then BXJ functions identically like a regular BX instruction.
    //
    // This is sufficient for citra, as the CPU for the 3DS does not implement Jazelle.

    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        bx_inst *const inst_cream = (bx_inst *)inst_base->component;

        std::uint32_t address = RM;

        if (inst_cream->Rm == 15)
            address += 2 * cpu->GetInstructionSize();

        cpu->TFlag = address & 1;
        cpu->Reg[15] = address & 0xfffffffe;
        INC_PC(sizeof(bx_inst));
        goto DISPATCH;
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(bx_inst));
    goto DISPATCH;
}

CDP_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        // Undefined instruction here
        cpu->NumInstrsToExecute = 0;
        return num_instrs;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(cdp_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

CLREX_INST : {
    cpu->exmonitor()->clear_exclusive();
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(clrex_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
CLZ_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        clz_inst *inst_cream = (clz_inst *)inst_base->component;
        RD = clz(RM);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(clz_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
CMN_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        cmn_inst *const inst_cream = (cmn_inst *)inst_base->component;

        std::uint32_t rn_val = RN;
        if (inst_cream->Rn == 15)
            rn_val += 2 * cpu->GetInstructionSize();

        bool carry;
        bool overflow;
        std::uint32_t result = AddWithCarry(rn_val, SHIFTER_OPERAND, 0, &carry, &overflow);

        UPDATE_NFLAG(result);
        UPDATE_ZFLAG(result);
        cpu->CFlag = carry;
        cpu->VFlag = overflow;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(cmn_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
CMP_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        cmp_inst *const inst_cream = (cmp_inst *)inst_base->component;

        std::uint32_t rn_val = RN;
        if (inst_cream->Rn == 15)
            rn_val += 2 * cpu->GetInstructionSize();

        bool carry;
        bool overflow;
        std::uint32_t result = AddWithCarry(rn_val, ~SHIFTER_OPERAND, 1, &carry, &overflow);

        UPDATE_NFLAG(result);
        UPDATE_ZFLAG(result);
        cpu->CFlag = carry;
        cpu->VFlag = overflow;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(cmp_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
CPS_INST : {
    cps_inst *inst_cream = (cps_inst *)inst_base->component;
    std::uint32_t aif_val = 0;
    std::uint32_t aif_mask = 0;
    if (cpu->InAPrivilegedMode()) {
        if (inst_cream->imod1) {
            if (inst_cream->A) {
                aif_val |= (inst_cream->imod0 << 8);
                aif_mask |= 1 << 8;
            }
            if (inst_cream->I) {
                aif_val |= (inst_cream->imod0 << 7);
                aif_mask |= 1 << 7;
            }
            if (inst_cream->F) {
                aif_val |= (inst_cream->imod0 << 6);
                aif_mask |= 1 << 6;
            }
            aif_mask = ~aif_mask;
            cpu->Cpsr = (cpu->Cpsr & aif_mask) | aif_val;
        }
        if (inst_cream->mmod) {
            cpu->Cpsr = (cpu->Cpsr & 0xffffffe0) | inst_cream->mode;
            cpu->ChangePrivilegeMode(inst_cream->mode);
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(cps_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
CPY_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        mov_inst *inst_cream = (mov_inst *)inst_base->component;

        RD = SHIFTER_OPERAND;
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(mov_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(mov_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
EOR_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        eor_inst *inst_cream = (eor_inst *)inst_base->component;

        std::uint32_t lop = RN;
        if (inst_cream->Rn == 15) {
            lop += 2 * cpu->GetInstructionSize();
        }
        std::uint32_t rop = SHIFTER_OPERAND;
        RD = lop ^ rop;
        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            UPDATE_CFLAG_WITH_SC;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(eor_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(eor_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDC_INST : {
    // Instruction not implemented
    // LOG_CRITICAL(eka2l1::CPU_DYNCOM, "unimplemented instruction");
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldc_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDM_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        inst_cream->get_addr(cpu, inst_cream->inst, addr);

        // The register list maps to contiguous, ascending addresses -- resolve the
        // host page once and reuse it for the whole run (see block_cursor).
        ARMul_State::block_cursor ldm_cur;

        unsigned int inst = inst_cream->inst;
        if (BIT(inst, 22) && !BIT(inst, 15)) {
            for (int i = 0; i < 13; i++) {
                if (BIT(inst, i)) {
                    cpu->Reg[i] = cpu->ReadMemory32Block(addr, ldm_cur);
                    addr += 4;
                }
            }
            if (BIT(inst, 13)) {
                if (cpu->Mode == USER32MODE)
                    cpu->Reg[13] = cpu->ReadMemory32Block(addr, ldm_cur);
                else
                    cpu->Reg_usr[0] = cpu->ReadMemory32Block(addr, ldm_cur);

                addr += 4;
            }
            if (BIT(inst, 14)) {
                if (cpu->Mode == USER32MODE)
                    cpu->Reg[14] = cpu->ReadMemory32Block(addr, ldm_cur);
                else
                    cpu->Reg_usr[1] = cpu->ReadMemory32Block(addr, ldm_cur);

                addr += 4;
            }
        } else if (!BIT(inst, 22)) {
            for (int i = 0; i < 16; i++) {
                if (BIT(inst, i)) {
                    unsigned int ret = cpu->ReadMemory32Block(addr, ldm_cur);

                    // For armv5t, should enter thumb when bits[0] is non-zero.
                    if (i == 15) {
                        cpu->TFlag = ret & 0x1;
                        ret &= 0xFFFFFFFE;
                    }

                    cpu->Reg[i] = ret;
                    addr += 4;
                }
            }
        } else if (BIT(inst, 22) && BIT(inst, 15)) {
            for (int i = 0; i < 15; i++) {
                if (BIT(inst, i)) {
                    cpu->Reg[i] = cpu->ReadMemory32Block(addr, ldm_cur);
                    addr += 4;
                }
            }

            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Cpsr & 0x1F);
                LOAD_NZCVT;
            }

            cpu->Reg[15] = cpu->ReadMemory32Block(addr, ldm_cur);
        }

        if (BIT(inst, 15)) {
            INC_PC(sizeof(ldst_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
SXTH_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        sxth_inst *inst_cream = (sxth_inst *)inst_base->component;

        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate);
        if (BIT(operand2, 15)) {
            operand2 |= 0xffff0000;
        } else {
            operand2 &= 0xffff;
        }
        RD = operand2;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(sxth_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDR_INST : {
    ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
    LS_GET_ADDR(addr);

    unsigned int value = cpu->ReadMemory32(addr);
    cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;

    if (BITS(inst_cream->inst, 12, 15) == 15) {
        // For armv5t, should enter thumb when bits[0] is non-zero.
        cpu->TFlag = value & 0x1;
        cpu->Reg[15] &= 0xFFFFFFFE;
        INC_PC(sizeof(ldst_inst));
        goto DISPATCH;
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDRCOND_INST : {
    if (CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        LS_GET_ADDR(addr);

        unsigned int value = cpu->ReadMemory32(addr);
        cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;

        if (BITS(inst_cream->inst, 12, 15) == 15) {
            // For armv5t, should enter thumb when bits[0] is non-zero.
            cpu->TFlag = value & 0x1;
            cpu->Reg[15] &= 0xFFFFFFFE;
            INC_PC(sizeof(ldst_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
UXTH_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        uxth_inst *inst_cream = (uxth_inst *)inst_base->component;
        RD = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) & 0xffff;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(uxth_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
UXTAH_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        uxtah_inst *inst_cream = (uxtah_inst *)inst_base->component;
        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) & 0xffff;

        RD = RN + operand2;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(uxtah_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDRB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        LS_GET_ADDR(addr);

        cpu->Reg[BITS(inst_cream->inst, 12, 15)] = cpu->ReadMemory8(addr);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDRBT_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        LS_GET_ADDR(addr);

        const std::uint32_t dest_index = BITS(inst_cream->inst, 12, 15);
        const std::uint32_t previous_mode = cpu->Mode;

        cpu->ChangePrivilegeMode(USER32MODE);
        const std::uint8_t value = cpu->ReadMemory8(addr);
        cpu->ChangePrivilegeMode(previous_mode);

        cpu->Reg[dest_index] = value;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDRD_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        // Should check if RD is even-numbered, Rd != 14, addr[0:1] == 0, (CP15_reg1_U == 1 ||
        // addr[2] == 0)
        MLS_GET_ADDR(addr);

        // The 3DS doesn't have LPAE (Large Physical Access Extension), so it
        // wouldn't do this as a single read.
        cpu->Reg[BITS(inst_cream->inst, 12, 15) + 0] = cpu->ReadMemory32(addr);
        cpu->Reg[BITS(inst_cream->inst, 12, 15) + 1] = cpu->ReadMemory32(addr + 4);

        // No dispatch since this operation should not modify R15
    }
    cpu->Reg[15] += 4;
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

LDREX_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *inst_cream = (generic_arm_inst *)inst_base->component;
        unsigned int read_addr = RN;

        RD = cpu->exmonitor()->exclusive_read32(cpu->parent(), read_addr);
        ;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDREXB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *inst_cream = (generic_arm_inst *)inst_base->component;
        unsigned int read_addr = RN;

        RD = cpu->exmonitor()->exclusive_read8(cpu->parent(), read_addr);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDREXH_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *inst_cream = (generic_arm_inst *)inst_base->component;
        unsigned int read_addr = RN;

        RD = cpu->exmonitor()->exclusive_read16(cpu->parent(), read_addr);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDREXD_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *inst_cream = (generic_arm_inst *)inst_base->component;
        unsigned int read_addr = RN;

        const std::uint64_t valval = cpu->exmonitor()->exclusive_read64(cpu->parent(), read_addr);

        RD = static_cast<std::uint32_t>(valval);
        RD2 = static_cast<std::uint32_t>(valval >> 32);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDRH_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        MLS_GET_ADDR(addr);

        cpu->Reg[BITS(inst_cream->inst, 12, 15)] = cpu->ReadMemory16(addr);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDRSB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        MLS_GET_ADDR(addr);
        unsigned int value = cpu->ReadMemory8(addr);
        if (BIT(value, 7)) {
            value |= 0xffffff00;
        }
        cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDRSH_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        MLS_GET_ADDR(addr);

        unsigned int value = cpu->ReadMemory16(addr);
        if (BIT(value, 15)) {
            value |= 0xffff0000;
        }
        cpu->Reg[BITS(inst_cream->inst, 12, 15)] = value;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
LDRT_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        LS_GET_ADDR(addr);

        const std::uint32_t dest_index = BITS(inst_cream->inst, 12, 15);
        const std::uint32_t previous_mode = cpu->Mode;

        cpu->ChangePrivilegeMode(USER32MODE);
        const std::uint32_t value = cpu->ReadMemory32(addr);
        cpu->ChangePrivilegeMode(previous_mode);

        cpu->Reg[dest_index] = value;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
MCR_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        mcr_inst *inst_cream = (mcr_inst *)inst_base->component;

        unsigned int inst = inst_cream->inst;
        if (inst_cream->Rd == 15) {
            DEBUG_MSG;
        } else {
            if (inst_cream->cp_num == 15)
                cpu->WriteCP15Register(RD, CRn, OPCODE_1, CRm, OPCODE_2);
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(mcr_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

MCRR_INST : {
    // Stubbed, as the MPCore doesn't have any registers that are accessible
    // through this instruction.
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        mcrr_inst *const inst_cream = (mcrr_inst *)inst_base->component;

        LOG_ERROR(eka2l1::CPU_DYNCOM, "MCRR executed | Coprocessor: {}, CRm {}, opc1: {}, Rt: {}, Rt2: {}",
            inst_cream->cp_num, inst_cream->crm, inst_cream->opcode_1, inst_cream->rt,
            inst_cream->rt2);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(mcrr_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

MLA_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        mla_inst *inst_cream = (mla_inst *)inst_base->component;

        std::uint64_t rm = RM;
        std::uint64_t rs = RS;
        std::uint64_t rn = RN;

        RD = static_cast<std::uint32_t>((rm * rs + rn) & 0xffffffff);
        if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(mla_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
MOV_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        mov_inst *inst_cream = (mov_inst *)inst_base->component;

        RD = SHIFTER_OPERAND;
        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            UPDATE_CFLAG_WITH_SC;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(mov_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(mov_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
MRC_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        mrc_inst *inst_cream = (mrc_inst *)inst_base->component;

        if (inst_cream->cp_num == 15) {
            const uint32_t value = cpu->ReadCP15Register(CRn, OPCODE_1, CRm, OPCODE_2);

            if (inst_cream->Rd == 15) {
                cpu->Cpsr = (cpu->Cpsr & ~0xF0000000) | (value & 0xF0000000);
                LOAD_NZCVT;
            } else {
                RD = value;
            }
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(mrc_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

MRRC_INST : {
    // Stubbed, as the MPCore doesn't have any registers that are accessible
    // through this instruction.
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        mcrr_inst *const inst_cream = (mcrr_inst *)inst_base->component;

        LOG_ERROR(eka2l1::CPU_DYNCOM, "MRRC executed | Coprocessor: {}, CRm {}, opc1: {}, Rt: {}, Rt2: {}",
            inst_cream->cp_num, inst_cream->crm, inst_cream->opcode_1, inst_cream->rt,
            inst_cream->rt2);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(mcrr_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

MRS_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        mrs_inst *inst_cream = (mrs_inst *)inst_base->component;

        if (inst_cream->R) {
            RD = cpu->Spsr_copy;
        } else {
            SAVE_NZCVT;
            RD = cpu->Cpsr;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(mrs_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
MSR_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        msr_inst *inst_cream = (msr_inst *)inst_base->component;
        const std::uint32_t UserMask = 0xf80f0200, PrivMask = 0x000001df, StateMask = 0x01000020;
        unsigned int inst = inst_cream->inst;
        unsigned int operand;

        if (BIT(inst, 25)) {
            int rot_imm = BITS(inst, 8, 11) * 2;
            operand = ROTATE_RIGHT_32(BITS(inst, 0, 7), rot_imm);
        } else {
            operand = cpu->Reg[BITS(inst, 0, 3)];
        }
        std::uint32_t byte_mask = (BIT(inst, 16) ? 0xff : 0) | (BIT(inst, 17) ? 0xff00 : 0) | (BIT(inst, 18) ? 0xff0000 : 0) | (BIT(inst, 19) ? 0xff000000 : 0);
        std::uint32_t mask = 0;
        if (!inst_cream->R) {
            if (cpu->InAPrivilegedMode()) {
                if ((operand & StateMask) != 0) {
                    /// UNPREDICTABLE
                    DEBUG_MSG;
                } else
                    mask = byte_mask & (UserMask | PrivMask);
            } else {
                mask = byte_mask & UserMask;
            }
            SAVE_NZCVT;

            cpu->Cpsr = (cpu->Cpsr & ~mask) | (operand & mask);
            cpu->ChangePrivilegeMode(cpu->Cpsr & 0x1F);
            LOAD_NZCVT;
        } else {
            if (cpu->CurrentModeHasSPSR()) {
                mask = byte_mask & (UserMask | PrivMask | StateMask);
                cpu->Spsr_copy = (cpu->Spsr_copy & ~mask) | (operand & mask);
            }
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(msr_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
MUL_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        mul_inst *inst_cream = (mul_inst *)inst_base->component;

        std::uint64_t rm = RM;
        std::uint64_t rs = RS;
        RD = static_cast<std::uint32_t>((rm * rs) & 0xffffffff);
        if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(mul_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
MVN_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        mvn_inst *const inst_cream = (mvn_inst *)inst_base->component;

        RD = ~SHIFTER_OPERAND;

        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            UPDATE_CFLAG_WITH_SC;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(mvn_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(mvn_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
ORR_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        orr_inst *const inst_cream = (orr_inst *)inst_base->component;

        std::uint32_t lop = RN;
        std::uint32_t rop = SHIFTER_OPERAND;

        if (inst_cream->Rn == 15)
            lop += 2 * cpu->GetInstructionSize();

        RD = lop | rop;

        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            UPDATE_CFLAG_WITH_SC;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(orr_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(orr_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

NOP_INST : {
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC_STUB;
    FETCH_INST;
    GOTO_NEXT_INST;
}

PKHBT_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        pkh_inst *inst_cream = (pkh_inst *)inst_base->component;
        RD = (RN & 0xFFFF) | ((RM << inst_cream->imm) & 0xFFFF0000);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(pkh_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

PKHTB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        pkh_inst *inst_cream = (pkh_inst *)inst_base->component;
        int shift_imm = inst_cream->imm ? inst_cream->imm : 31;
        RD = ((static_cast<std::int32_t>(RM) >> shift_imm) & 0xFFFF) | (RN & 0xFFFF0000);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(pkh_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

PLD_INST : {
    // Not implemented. PLD is a hint instruction, so it's optional.

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(pld_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

QADD_INST:
QDADD_INST:
QDSUB_INST:
QSUB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *const inst_cream = (generic_arm_inst *)inst_base->component;
        const std::uint8_t op1 = inst_cream->op1;
        const std::uint32_t rm_val = RM;
        const std::uint32_t rn_val = RN;

        std::uint32_t result = 0;

        // QADD
        if (op1 == 0x00) {
            result = rm_val + rn_val;

            if (AddOverflow(rm_val, rn_val, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpu->Cpsr |= (1 << 27);
            }
        }
        // QSUB
        else if (op1 == 0x01) {
            result = rm_val - rn_val;

            if (SubOverflow(rm_val, rn_val, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpu->Cpsr |= (1 << 27);
            }
        }
        // QDADD
        else if (op1 == 0x02) {
            std::uint32_t mul = (rn_val * 2);

            if (AddOverflow(rn_val, rn_val, rn_val * 2)) {
                mul = POS(mul) ? 0x80000000 : 0x7FFFFFFF;
                cpu->Cpsr |= (1 << 27);
            }

            result = mul + rm_val;

            if (AddOverflow(rm_val, mul, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpu->Cpsr |= (1 << 27);
            }
        }
        // QDSUB
        else if (op1 == 0x03) {
            std::uint32_t mul = (rn_val * 2);

            if (AddOverflow(rn_val, rn_val, mul)) {
                mul = POS(mul) ? 0x80000000 : 0x7FFFFFFF;
                cpu->Cpsr |= (1 << 27);
            }

            result = rm_val - mul;

            if (SubOverflow(rm_val, mul, result)) {
                result = POS(result) ? 0x80000000 : 0x7FFFFFFF;
                cpu->Cpsr |= (1 << 27);
            }
        }

        RD = result;
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

QADD8_INST:
QADD16_INST:
QADDSUBX_INST:
QSUB8_INST:
QSUB16_INST:
QSUBADDX_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *const inst_cream = (generic_arm_inst *)inst_base->component;
        const std::uint16_t rm_lo = (RM & 0xFFFF);
        const std::uint16_t rm_hi = ((RM >> 16) & 0xFFFF);
        const std::uint16_t rn_lo = (RN & 0xFFFF);
        const std::uint16_t rn_hi = ((RN >> 16) & 0xFFFF);
        const std::uint8_t op2 = inst_cream->op2;

        std::uint16_t lo_result = 0;
        std::uint16_t hi_result = 0;

        // QADD16
        if (op2 == 0x00) {
            lo_result = ARMul_SignedSaturatedAdd16(rn_lo, rm_lo);
            hi_result = ARMul_SignedSaturatedAdd16(rn_hi, rm_hi);
        }
        // QASX
        else if (op2 == 0x01) {
            lo_result = ARMul_SignedSaturatedSub16(rn_lo, rm_hi);
            hi_result = ARMul_SignedSaturatedAdd16(rn_hi, rm_lo);
        }
        // QSAX
        else if (op2 == 0x02) {
            lo_result = ARMul_SignedSaturatedAdd16(rn_lo, rm_hi);
            hi_result = ARMul_SignedSaturatedSub16(rn_hi, rm_lo);
        }
        // QSUB16
        else if (op2 == 0x03) {
            lo_result = ARMul_SignedSaturatedSub16(rn_lo, rm_lo);
            hi_result = ARMul_SignedSaturatedSub16(rn_hi, rm_hi);
        }
        // QADD8
        else if (op2 == 0x04) {
            lo_result = ARMul_SignedSaturatedAdd8(rn_lo & 0xFF, rm_lo & 0xFF) | ARMul_SignedSaturatedAdd8(rn_lo >> 8, rm_lo >> 8) << 8;
            hi_result = ARMul_SignedSaturatedAdd8(rn_hi & 0xFF, rm_hi & 0xFF) | ARMul_SignedSaturatedAdd8(rn_hi >> 8, rm_hi >> 8) << 8;
        }
        // QSUB8
        else if (op2 == 0x07) {
            lo_result = ARMul_SignedSaturatedSub8(rn_lo & 0xFF, rm_lo & 0xFF) | ARMul_SignedSaturatedSub8(rn_lo >> 8, rm_lo >> 8) << 8;
            hi_result = ARMul_SignedSaturatedSub8(rn_hi & 0xFF, rm_hi & 0xFF) | ARMul_SignedSaturatedSub8(rn_hi >> 8, rm_hi >> 8) << 8;
        }

        RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

REV_INST:
REV16_INST:
REVSH_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        rev_inst *const inst_cream = (rev_inst *)inst_base->component;

        const std::uint8_t op1 = inst_cream->op1;
        const std::uint8_t op2 = inst_cream->op2;

        // REV
        if (op1 == 0x03 && op2 == 0x01) {
            RD = ((RM & 0xFF) << 24) | (((RM >> 8) & 0xFF) << 16) | (((RM >> 16) & 0xFF) << 8) | ((RM >> 24) & 0xFF);
        }
        // REV16
        else if (op1 == 0x03 && op2 == 0x05) {
            RD = ((RM & 0xFF) << 8) | ((RM & 0xFF00) >> 8) | ((RM & 0xFF0000) << 8) | ((RM & 0xFF000000) >> 8);
        }
        // REVSH
        else if (op1 == 0x07 && op2 == 0x05) {
            RD = ((RM & 0xFF) << 8) | ((RM & 0xFF00) >> 8);
            if (RD & 0x8000)
                RD |= 0xffff0000;
        }
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(rev_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

RFE_INST : {
    // RFE is unconditional
    ldst_inst *const inst_cream = (ldst_inst *)inst_base->component;

    std::uint32_t address = 0;
    inst_cream->get_addr(cpu, inst_cream->inst, address);

    cpu->Cpsr = cpu->ReadMemory32(address);
    cpu->Reg[15] = cpu->ReadMemory32(address + 4);

    INC_PC(sizeof(ldst_inst));
    goto DISPATCH;
}

RSB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        rsb_inst *const inst_cream = (rsb_inst *)inst_base->component;

        std::uint32_t rn_val = RN;
        if (inst_cream->Rn == 15)
            rn_val += 2 * cpu->GetInstructionSize();

        bool carry;
        bool overflow;
        RD = AddWithCarry(~rn_val, SHIFTER_OPERAND, 1, &carry, &overflow);

        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            cpu->CFlag = carry;
            cpu->VFlag = overflow;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(rsb_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(rsb_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
RSC_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        rsc_inst *const inst_cream = (rsc_inst *)inst_base->component;

        std::uint32_t rn_val = RN;
        if (inst_cream->Rn == 15)
            rn_val += 2 * cpu->GetInstructionSize();

        bool carry;
        bool overflow;
        RD = AddWithCarry(~rn_val, SHIFTER_OPERAND, cpu->CFlag, &carry, &overflow);

        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            cpu->CFlag = carry;
            cpu->VFlag = overflow;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(rsc_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(rsc_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SADD8_INST:
SSUB8_INST:
SADD16_INST:
SADDSUBX_INST:
SSUBADDX_INST:
SSUB16_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *const inst_cream = (generic_arm_inst *)inst_base->component;
        const std::uint8_t op2 = inst_cream->op2;

        if (op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03) {
            const std::int16_t rn_lo = (RN & 0xFFFF);
            const std::int16_t rn_hi = ((RN >> 16) & 0xFFFF);
            const std::int16_t rm_lo = (RM & 0xFFFF);
            const std::int16_t rm_hi = ((RM >> 16) & 0xFFFF);

            std::int32_t lo_result = 0;
            std::int32_t hi_result = 0;

            // SADD16
            if (inst_cream->op2 == 0x00) {
                lo_result = (rn_lo + rm_lo);
                hi_result = (rn_hi + rm_hi);
            }
            // SASX
            else if (op2 == 0x01) {
                lo_result = (rn_lo - rm_hi);
                hi_result = (rn_hi + rm_lo);
            }
            // SSAX
            else if (op2 == 0x02) {
                lo_result = (rn_lo + rm_hi);
                hi_result = (rn_hi - rm_lo);
            }
            // SSUB16
            else if (op2 == 0x03) {
                lo_result = (rn_lo - rm_lo);
                hi_result = (rn_hi - rm_hi);
            }

            RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);

            if (lo_result >= 0) {
                cpu->Cpsr |= (1 << 16);
                cpu->Cpsr |= (1 << 17);
            } else {
                cpu->Cpsr &= ~(1 << 16);
                cpu->Cpsr &= ~(1 << 17);
            }

            if (hi_result >= 0) {
                cpu->Cpsr |= (1 << 18);
                cpu->Cpsr |= (1 << 19);
            } else {
                cpu->Cpsr &= ~(1 << 18);
                cpu->Cpsr &= ~(1 << 19);
            }
        } else if (op2 == 0x04 || op2 == 0x07) {
            std::int32_t lo_val1, lo_val2;
            std::int32_t hi_val1, hi_val2;

            // SADD8
            if (op2 == 0x04) {
                lo_val1 = (std::int32_t)(std::int8_t)(RN & 0xFF) + (std::int32_t)(std::int8_t)(RM & 0xFF);
                lo_val2 = (std::int32_t)(std::int8_t)((RN >> 8) & 0xFF) + (std::int32_t)(std::int8_t)((RM >> 8) & 0xFF);
                hi_val1 = (std::int32_t)(std::int8_t)((RN >> 16) & 0xFF) + (std::int32_t)(std::int8_t)((RM >> 16) & 0xFF);
                hi_val2 = (std::int32_t)(std::int8_t)((RN >> 24) & 0xFF) + (std::int32_t)(std::int8_t)((RM >> 24) & 0xFF);
            }
            // SSUB8
            else {
                lo_val1 = (std::int32_t)(std::int8_t)(RN & 0xFF) - (std::int32_t)(std::int8_t)(RM & 0xFF);
                lo_val2 = (std::int32_t)(std::int8_t)((RN >> 8) & 0xFF) - (std::int32_t)(std::int8_t)((RM >> 8) & 0xFF);
                hi_val1 = (std::int32_t)(std::int8_t)((RN >> 16) & 0xFF) - (std::int32_t)(std::int8_t)((RM >> 16) & 0xFF);
                hi_val2 = (std::int32_t)(std::int8_t)((RN >> 24) & 0xFF) - (std::int32_t)(std::int8_t)((RM >> 24) & 0xFF);
            }

            RD = ((lo_val1 & 0xFF) | ((lo_val2 & 0xFF) << 8) | ((hi_val1 & 0xFF) << 16) | ((hi_val2 & 0xFF) << 24));

            if (lo_val1 >= 0)
                cpu->Cpsr |= (1 << 16);
            else
                cpu->Cpsr &= ~(1 << 16);

            if (lo_val2 >= 0)
                cpu->Cpsr |= (1 << 17);
            else
                cpu->Cpsr &= ~(1 << 17);

            if (hi_val1 >= 0)
                cpu->Cpsr |= (1 << 18);
            else
                cpu->Cpsr &= ~(1 << 18);

            if (hi_val2 >= 0)
                cpu->Cpsr |= (1 << 19);
            else
                cpu->Cpsr &= ~(1 << 19);
        }
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SBC_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        sbc_inst *const inst_cream = (sbc_inst *)inst_base->component;

        std::uint32_t rn_val = RN;
        if (inst_cream->Rn == 15)
            rn_val += 2 * cpu->GetInstructionSize();

        bool carry;
        bool overflow;
        RD = AddWithCarry(rn_val, ~SHIFTER_OPERAND, cpu->CFlag, &carry, &overflow);

        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            cpu->CFlag = carry;
            cpu->VFlag = overflow;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(sbc_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(sbc_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SEL_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *const inst_cream = (generic_arm_inst *)inst_base->component;

        const std::uint32_t to = RM;
        const std::uint32_t from = RN;
        const std::uint32_t cpsr = cpu->Cpsr;

        std::uint32_t result;
        if (cpsr & (1 << 16))
            result = from & 0xff;
        else
            result = to & 0xff;

        if (cpsr & (1 << 17))
            result |= from & 0x0000ff00;
        else
            result |= to & 0x0000ff00;

        if (cpsr & (1 << 18))
            result |= from & 0x00ff0000;
        else
            result |= to & 0x00ff0000;

        if (cpsr & (1 << 19))
            result |= from & 0xff000000;
        else
            result |= to & 0xff000000;

        RD = result;
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SETEND_INST : {
    // SETEND is unconditional
    setend_inst *const inst_cream = (setend_inst *)inst_base->component;
    const bool big_endian = (inst_cream->set_bigend == 1);

    if (big_endian)
        cpu->Cpsr |= (1 << 9);
    else
        cpu->Cpsr &= ~(1 << 9);

    LOG_WARN(eka2l1::CPU_DYNCOM, "SETEND {} executed", big_endian ? "BE" : "LE");

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(setend_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SEV_INST : {
    // Stubbed, as SEV is a hint instruction.
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        LOG_TRACE(eka2l1::CPU_DYNCOM, "SEV executed.");
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC_STUB;
    FETCH_INST;
    GOTO_NEXT_INST;
}

SHADD8_INST:
SHADD16_INST:
SHADDSUBX_INST:
SHSUB8_INST:
SHSUB16_INST:
SHSUBADDX_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *const inst_cream = (generic_arm_inst *)inst_base->component;

        const std::uint8_t op2 = inst_cream->op2;
        const std::uint32_t rm_val = RM;
        const std::uint32_t rn_val = RN;

        if (op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03) {
            std::int32_t lo_result = 0;
            std::int32_t hi_result = 0;

            // SHADD16
            if (op2 == 0x00) {
                lo_result = ((std::int16_t)(rn_val & 0xFFFF) + (std::int16_t)(rm_val & 0xFFFF)) >> 1;
                hi_result = ((std::int16_t)((rn_val >> 16) & 0xFFFF) + (std::int16_t)((rm_val >> 16) & 0xFFFF)) >> 1;
            }
            // SHASX
            else if (op2 == 0x01) {
                lo_result = ((std::int16_t)(rn_val & 0xFFFF) - (std::int16_t)((rm_val >> 16) & 0xFFFF)) >> 1;
                hi_result = ((std::int16_t)((rn_val >> 16) & 0xFFFF) + (std::int16_t)(rm_val & 0xFFFF)) >> 1;
            }
            // SHSAX
            else if (op2 == 0x02) {
                lo_result = ((std::int16_t)(rn_val & 0xFFFF) + (std::int16_t)((rm_val >> 16) & 0xFFFF)) >> 1;
                hi_result = ((std::int16_t)((rn_val >> 16) & 0xFFFF) - (std::int16_t)(rm_val & 0xFFFF)) >> 1;
            }
            // SHSUB16
            else if (op2 == 0x03) {
                lo_result = ((std::int16_t)(rn_val & 0xFFFF) - (std::int16_t)(rm_val & 0xFFFF)) >> 1;
                hi_result = ((std::int16_t)((rn_val >> 16) & 0xFFFF) - (std::int16_t)((rm_val >> 16) & 0xFFFF)) >> 1;
            }

            RD = ((lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16));
        } else if (op2 == 0x04 || op2 == 0x07) {
            std::int16_t lo_val1, lo_val2;
            std::int16_t hi_val1, hi_val2;

            // SHADD8
            if (op2 == 0x04) {
                lo_val1 = ((std::int8_t)(rn_val & 0xFF) + (std::int8_t)(rm_val & 0xFF)) >> 1;
                lo_val2 = ((std::int8_t)((rn_val >> 8) & 0xFF) + (std::int8_t)((rm_val >> 8) & 0xFF)) >> 1;

                hi_val1 = ((std::int8_t)((rn_val >> 16) & 0xFF) + (std::int8_t)((rm_val >> 16) & 0xFF)) >> 1;
                hi_val2 = ((std::int8_t)((rn_val >> 24) & 0xFF) + (std::int8_t)((rm_val >> 24) & 0xFF)) >> 1;
            }
            // SHSUB8
            else {
                lo_val1 = ((std::int8_t)(rn_val & 0xFF) - (std::int8_t)(rm_val & 0xFF)) >> 1;
                lo_val2 = ((std::int8_t)((rn_val >> 8) & 0xFF) - (std::int8_t)((rm_val >> 8) & 0xFF)) >> 1;

                hi_val1 = ((std::int8_t)((rn_val >> 16) & 0xFF) - (std::int8_t)((rm_val >> 16) & 0xFF)) >> 1;
                hi_val2 = ((std::int8_t)((rn_val >> 24) & 0xFF) - (std::int8_t)((rm_val >> 24) & 0xFF)) >> 1;
            }

            RD = (lo_val1 & 0xFF) | ((lo_val2 & 0xFF) << 8) | ((hi_val1 & 0xFF) << 16) | ((hi_val2 & 0xFF) << 24);
        }
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SMLA_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        smla_inst *inst_cream = (smla_inst *)inst_base->component;
        std::int32_t operand1, operand2;
        if (inst_cream->x == 0)
            operand1 = (BIT(RM, 15)) ? (BITS(RM, 0, 15) | 0xffff0000) : BITS(RM, 0, 15);
        else
            operand1 = (BIT(RM, 31)) ? (BITS(RM, 16, 31) | 0xffff0000) : BITS(RM, 16, 31);

        if (inst_cream->y == 0)
            operand2 = (BIT(RS, 15)) ? (BITS(RS, 0, 15) | 0xffff0000) : BITS(RS, 0, 15);
        else
            operand2 = (BIT(RS, 31)) ? (BITS(RS, 16, 31) | 0xffff0000) : BITS(RS, 16, 31);

        std::uint32_t product = operand1 * operand2;
        std::uint32_t result = product + RN;
        if (AddOverflow(product, RN, result))
            cpu->Cpsr |= (1 << 27);
        RD = result;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(smla_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SMLAD_INST:
SMLSD_INST:
SMUAD_INST:
SMUSD_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        smlad_inst *const inst_cream = (smlad_inst *)inst_base->component;
        const std::uint8_t op2 = inst_cream->op2;

        std::uint32_t rm_val = cpu->Reg[inst_cream->Rm];
        const std::uint32_t rn_val = cpu->Reg[inst_cream->Rn];

        if (inst_cream->m)
            rm_val = (((rm_val & 0xFFFF) << 16) | (rm_val >> 16));

        const std::int16_t rm_lo = (rm_val & 0xFFFF);
        const std::int16_t rm_hi = ((rm_val >> 16) & 0xFFFF);
        const std::int16_t rn_lo = (rn_val & 0xFFFF);
        const std::int16_t rn_hi = ((rn_val >> 16) & 0xFFFF);

        const std::uint32_t product1 = (rn_lo * rm_lo);
        const std::uint32_t product2 = (rn_hi * rm_hi);

        // SMUAD and SMLAD
        if (BIT(op2, 1) == 0) {
            std::uint32_t rd_val = (product1 + product2);

            if (inst_cream->Ra != 15) {
                rd_val += cpu->Reg[inst_cream->Ra];

                if (ARMul_AddOverflowQ(product1 + product2, cpu->Reg[inst_cream->Ra]))
                    cpu->Cpsr |= (1 << 27);
            }

            RD = rd_val;

            if (ARMul_AddOverflowQ(product1, product2))
                cpu->Cpsr |= (1 << 27);
        }
        // SMUSD and SMLSD
        else {
            std::uint32_t rd_val = (product1 - product2);

            if (inst_cream->Ra != 15) {
                rd_val += cpu->Reg[inst_cream->Ra];

                if (ARMul_AddOverflowQ(product1 - product2, cpu->Reg[inst_cream->Ra]))
                    cpu->Cpsr |= (1 << 27);
            }

            RD = rd_val;
        }
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(smlad_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SMLAL_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        umlal_inst *inst_cream = (umlal_inst *)inst_base->component;
        long long int rm = RM;
        long long int rs = RS;
        if (BIT(rm, 31)) {
            rm |= 0xffffffff00000000LL;
        }
        if (BIT(rs, 31)) {
            rs |= 0xffffffff00000000LL;
        }
        long long int rst = rm * rs;
        long long int rdhi32 = RDHI;
        long long int hilo = (rdhi32 << 32) + RDLO;
        rst += hilo;
        RDLO = BITS(rst, 0, 31);
        RDHI = BITS(rst, 32, 63);
        if (inst_cream->S) {
            cpu->NFlag = BIT(RDHI, 31);
            cpu->ZFlag = (RDHI == 0 && RDLO == 0);
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(umlal_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SMLALXY_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        smlalxy_inst *const inst_cream = (smlalxy_inst *)inst_base->component;

        std::uint64_t operand1 = RN;
        std::uint64_t operand2 = RM;

        if (inst_cream->x != 0)
            operand1 >>= 16;
        if (inst_cream->y != 0)
            operand2 >>= 16;
        operand1 &= 0xFFFF;
        if (operand1 & 0x8000)
            operand1 -= 65536;
        operand2 &= 0xFFFF;
        if (operand2 & 0x8000)
            operand2 -= 65536;

        std::uint64_t dest = ((std::uint64_t)RDHI << 32 | RDLO) + (operand1 * operand2);
        RDLO = (dest & 0xFFFFFFFF);
        RDHI = ((dest >> 32) & 0xFFFFFFFF);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(smlalxy_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SMLAW_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        smlad_inst *const inst_cream = (smlad_inst *)inst_base->component;

        const std::uint32_t rm_val = RM;
        const std::uint32_t rn_val = RN;
        const std::uint32_t ra_val = cpu->Reg[inst_cream->Ra];
        const bool high = (inst_cream->m == 1);

        const std::int16_t operand2 = (high) ? ((rm_val >> 16) & 0xFFFF) : (rm_val & 0xFFFF);
        const std::int64_t result = (std::int64_t)(std::int32_t)rn_val * (std::int64_t)(std::int32_t)operand2 + ((std::int64_t)(std::int32_t)ra_val << 16);

        RD = BITS(result, 16, 47);

        if ((result >> 16) != (std::int32_t)RD)
            cpu->Cpsr |= (1 << 27);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(smlad_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SMLALD_INST:
SMLSLD_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        smlald_inst *const inst_cream = (smlald_inst *)inst_base->component;

        const bool do_swap = (inst_cream->swap == 1);
        const std::uint32_t rdlo_val = RDLO;
        const std::uint32_t rdhi_val = RDHI;
        const std::uint32_t rn_val = RN;
        std::uint32_t rm_val = RM;

        if (do_swap)
            rm_val = (((rm_val & 0xFFFF) << 16) | (rm_val >> 16));

        const std::int32_t product1 = (std::int16_t)(rn_val & 0xFFFF) * (std::int16_t)(rm_val & 0xFFFF);
        const std::int32_t product2 = (std::int16_t)((rn_val >> 16) & 0xFFFF) * (std::int16_t)((rm_val >> 16) & 0xFFFF);
        std::int64_t result;

        // SMLALD
        if (BIT(inst_cream->op2, 1) == 0) {
            result = (product1 + product2) + (std::int64_t)(rdlo_val | ((std::int64_t)rdhi_val << 32));
        }
        // SMLSLD
        else {
            result = (product1 - product2) + (std::int64_t)(rdlo_val | ((std::int64_t)rdhi_val << 32));
        }

        RDLO = (result & 0xFFFFFFFF);
        RDHI = ((result >> 32) & 0xFFFFFFFF);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(smlald_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SMMLA_INST:
SMMLS_INST:
SMMUL_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        smlad_inst *const inst_cream = (smlad_inst *)inst_base->component;

        const std::uint32_t rm_val = RM;
        const std::uint32_t rn_val = RN;
        const bool do_round = (inst_cream->m == 1);

        // Assume SMMUL by default.
        std::int64_t result = (std::int64_t)(std::int32_t)rn_val * (std::int64_t)(std::int32_t)rm_val;

        if (inst_cream->Ra != 15) {
            const std::uint32_t ra_val = cpu->Reg[inst_cream->Ra];

            // SMMLA, otherwise SMMLS
            if (BIT(inst_cream->op2, 1) == 0)
                result += ((std::int64_t)ra_val << 32);
            else
                result = ((std::int64_t)ra_val << 32) - result;
        }

        if (do_round)
            result += 0x80000000;

        RD = ((result >> 32) & 0xFFFFFFFF);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(smlad_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SMUL_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        smul_inst *inst_cream = (smul_inst *)inst_base->component;
        std::uint32_t operand1, operand2;
        if (inst_cream->x == 0)
            operand1 = (BIT(RM, 15)) ? (BITS(RM, 0, 15) | 0xffff0000) : BITS(RM, 0, 15);
        else
            operand1 = (BIT(RM, 31)) ? (BITS(RM, 16, 31) | 0xffff0000) : BITS(RM, 16, 31);

        if (inst_cream->y == 0)
            operand2 = (BIT(RS, 15)) ? (BITS(RS, 0, 15) | 0xffff0000) : BITS(RS, 0, 15);
        else
            operand2 = (BIT(RS, 31)) ? (BITS(RS, 16, 31) | 0xffff0000) : BITS(RS, 16, 31);
        RD = operand1 * operand2;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(smul_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
SMULL_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        umull_inst *inst_cream = (umull_inst *)inst_base->component;
        std::int64_t rm = RM;
        std::int64_t rs = RS;
        if (BIT(rm, 31)) {
            rm |= 0xffffffff00000000LL;
        }
        if (BIT(rs, 31)) {
            rs |= 0xffffffff00000000LL;
        }
        std::int64_t rst = rm * rs;
        RDHI = BITS(rst, 32, 63);
        RDLO = BITS(rst, 0, 31);

        if (inst_cream->S) {
            cpu->NFlag = BIT(RDHI, 31);
            cpu->ZFlag = (RDHI == 0 && RDLO == 0);
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(umull_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SMULW_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        smlad_inst *const inst_cream = (smlad_inst *)inst_base->component;

        std::int16_t rm = (inst_cream->m == 1) ? ((RM >> 16) & 0xFFFF) : (RM & 0xFFFF);

        std::int64_t result = (std::int64_t)rm * (std::int64_t)(std::int32_t)RN;
        RD = BITS(result, 16, 47);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(smlad_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SRS_INST : {
    // SRS is unconditional
    ldst_inst *const inst_cream = (ldst_inst *)inst_base->component;

    std::uint32_t address = 0;
    inst_cream->get_addr(cpu, inst_cream->inst, address);

    cpu->WriteMemory32(address + 0, cpu->Reg[14]);
    cpu->WriteMemory32(address + 4, cpu->Spsr_copy);

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SSAT_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ssat_inst *const inst_cream = (ssat_inst *)inst_base->component;

        std::uint8_t shift_type = inst_cream->shift_type;
        std::uint8_t shift_amount = inst_cream->imm5;
        std::uint32_t rn_val = RN;

        // 32-bit ASR is encoded as an amount of 0.
        if (shift_type == 1 && shift_amount == 0)
            shift_amount = 31;

        if (shift_type == 0)
            rn_val <<= shift_amount;
        else if (shift_type == 1)
            rn_val = ((std::int32_t)rn_val >> shift_amount);

        bool saturated = false;
        rn_val = ARMul_SignedSatQ(rn_val, inst_cream->sat_imm, &saturated);

        if (saturated)
            cpu->Cpsr |= (1 << 27);

        RD = rn_val;
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ssat_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SSAT16_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ssat_inst *const inst_cream = (ssat_inst *)inst_base->component;
        const std::uint8_t saturate_to = inst_cream->sat_imm;

        bool sat1 = false;
        bool sat2 = false;

        RD = (ARMul_SignedSatQ((std::int16_t)RN, saturate_to, &sat1) & 0xFFFF) | ARMul_SignedSatQ((std::int32_t)RN >> 16, saturate_to, &sat2) << 16;

        if (sat1 || sat2)
            cpu->Cpsr |= (1 << 27);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ssat_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

STC_INST : {
    // Instruction not implemented
    // LOG_CRITICAL(eka2l1::CPU_DYNCOM, "unimplemented instruction");
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(stc_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STM_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        unsigned int inst = inst_cream->inst;

        unsigned int Rn = BITS(inst, 16, 19);
        unsigned int old_RN = cpu->Reg[Rn];

        inst_cream->get_addr(cpu, inst_cream->inst, addr);

        // Contiguous, ascending stores -- resolve the host page once (see block_cursor).
        ARMul_State::block_cursor stm_cur;

        if (BIT(inst_cream->inst, 22) == 1) {
            for (int i = 0; i < 13; i++) {
                if (BIT(inst_cream->inst, i)) {
                    cpu->WriteMemory32Block(addr, cpu->Reg[i], stm_cur);
                    addr += 4;
                }
            }
            if (BIT(inst_cream->inst, 13)) {
                if (cpu->Mode == USER32MODE)
                    cpu->WriteMemory32Block(addr, cpu->Reg[13], stm_cur);
                else
                    cpu->WriteMemory32Block(addr, cpu->Reg_usr[0], stm_cur);

                addr += 4;
            }
            if (BIT(inst_cream->inst, 14)) {
                if (cpu->Mode == USER32MODE)
                    cpu->WriteMemory32Block(addr, cpu->Reg[14], stm_cur);
                else
                    cpu->WriteMemory32Block(addr, cpu->Reg_usr[1], stm_cur);

                addr += 4;
            }
            if (BIT(inst_cream->inst, 15)) {
                cpu->WriteMemory32Block(addr, cpu->Reg[15] + 8, stm_cur);
            }
        } else {
            for (unsigned int i = 0; i < 15; i++) {
                if (BIT(inst_cream->inst, i)) {
                    if (i == Rn)
                        cpu->WriteMemory32Block(addr, old_RN, stm_cur);
                    else
                        cpu->WriteMemory32Block(addr, cpu->Reg[i], stm_cur);

                    addr += 4;
                }
            }

            // Check PC reg
            if (BIT(inst_cream->inst, 15)) {
                cpu->WriteMemory32Block(addr, cpu->Reg[15] + 8, stm_cur);
            }
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
SXTB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        sxtb_inst *inst_cream = (sxtb_inst *)inst_base->component;

        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate);
        if (BIT(operand2, 7)) {
            operand2 |= 0xffffff00;
        } else {
            operand2 &= 0xff;
        }
        RD = operand2;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(sxtb_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STR_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        LS_GET_ADDR(addr);

        unsigned int reg = BITS(inst_cream->inst, 12, 15);
        unsigned int value = cpu->Reg[reg];

        if (reg == 15)
            value += 2 * cpu->GetInstructionSize();

        cpu->WriteMemory32(addr, value);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
UXTB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        uxtb_inst *inst_cream = (uxtb_inst *)inst_base->component;
        RD = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) & 0xff;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(uxtb_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
UXTAB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        uxtab_inst *inst_cream = (uxtab_inst *)inst_base->component;

        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) & 0xff;
        RD = RN + operand2;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(uxtab_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STRB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        LS_GET_ADDR(addr);
        unsigned int value = cpu->Reg[BITS(inst_cream->inst, 12, 15)] & 0xff;
        cpu->WriteMemory8(addr, value);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STRBT_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        LS_GET_ADDR(addr);

        const std::uint32_t previous_mode = cpu->Mode;
        const std::uint32_t value = cpu->Reg[BITS(inst_cream->inst, 12, 15)] & 0xff;

        cpu->ChangePrivilegeMode(USER32MODE);
        cpu->WriteMemory8(addr, value);
        cpu->ChangePrivilegeMode(previous_mode);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STRD_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        MLS_GET_ADDR(addr);

        // The 3DS doesn't have the Large Physical Access Extension (LPAE)
        // so STRD wouldn't store these as a single write.
        cpu->WriteMemory32(addr + 0, cpu->Reg[BITS(inst_cream->inst, 12, 15)]);
        cpu->WriteMemory32(addr + 4, cpu->Reg[BITS(inst_cream->inst, 12, 15) + 1]);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STREX_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *inst_cream = (generic_arm_inst *)inst_base->component;
        unsigned int write_addr = cpu->Reg[inst_cream->Rn];

        RD = (cpu->exmonitor()->exclusive_write32(cpu->parent(), write_addr, RM) ? 0 : 1);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STREXB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *inst_cream = (generic_arm_inst *)inst_base->component;
        unsigned int write_addr = cpu->Reg[inst_cream->Rn];

        RD = (cpu->exmonitor()->exclusive_write8(cpu->parent(), write_addr, RM) ? 0 : 1);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STREXD_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *inst_cream = (generic_arm_inst *)inst_base->component;
        unsigned int write_addr = cpu->Reg[inst_cream->Rn];

        const std::uint32_t rt = cpu->Reg[inst_cream->Rm + 0];
        const std::uint32_t rt2 = cpu->Reg[inst_cream->Rm + 1];
        std::uint64_t value;

        if (cpu->InBigEndianMode())
            value = (((std::uint64_t)rt << 32) | rt2);
        else
            value = (((std::uint64_t)rt2 << 32) | rt);

        RD = (cpu->exmonitor()->exclusive_write64(cpu->parent(), write_addr, RM) ? 0 : 1);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STREXH_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *inst_cream = (generic_arm_inst *)inst_base->component;
        unsigned int write_addr = cpu->Reg[inst_cream->Rn];

        RD = (cpu->exmonitor()->exclusive_write16(cpu->parent(), write_addr, RM) ? 0 : 1);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STRH_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        MLS_GET_ADDR(addr);

        unsigned int value = cpu->Reg[BITS(inst_cream->inst, 12, 15)] & 0xffff;
        cpu->WriteMemory16(addr, value);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
STRT_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ldst_inst *inst_cream = (ldst_inst *)inst_base->component;
        LS_GET_ADDR(addr);

        const std::uint32_t previous_mode = cpu->Mode;
        const std::uint32_t rt_index = BITS(inst_cream->inst, 12, 15);

        std::uint32_t value = cpu->Reg[rt_index];
        if (rt_index == 15)
            value += 2 * cpu->GetInstructionSize();

        cpu->ChangePrivilegeMode(USER32MODE);
        cpu->WriteMemory32(addr, value);
        cpu->ChangePrivilegeMode(previous_mode);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ldst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
SUB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        sub_inst *const inst_cream = (sub_inst *)inst_base->component;

        std::uint32_t rn_val = CHECK_READ_REG15(cpu, inst_cream->Rn);

        bool carry;
        bool overflow;
        RD = AddWithCarry(rn_val, ~SHIFTER_OPERAND, 1, &carry, &overflow);

        if (inst_cream->S && (inst_cream->Rd == 15)) {
            if (cpu->CurrentModeHasSPSR()) {
                cpu->Cpsr = cpu->Spsr_copy;
                cpu->ChangePrivilegeMode(cpu->Spsr_copy & 0x1F);
                LOAD_NZCVT;
            }
        } else if (inst_cream->S) {
            UPDATE_NFLAG(RD);
            UPDATE_ZFLAG(RD);
            cpu->CFlag = carry;
            cpu->VFlag = overflow;
        }
        if (inst_cream->Rd == 15) {
            INC_PC(sizeof(sub_inst));
            goto DISPATCH;
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(sub_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
SWI_INST : {
    // Increase the PC first
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(swi_inst));

    // Some system calls modify PC, so do this first
    const std::uint32_t current_pc = cpu->Reg[15];

    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        SAVE_NZCVT;

        swi_inst *const inst_cream = (swi_inst *)inst_base->component;
        cpu->NumInstrsToExecute = num_instrs >= cpu->NumInstrsToExecute ? 0 : cpu->NumInstrsToExecute - num_instrs;
        cpu->RaiseSystemCall(inst_cream->num);
        // The kernel would call ERET to get here, which clears exclusive memory state.
        cpu->exmonitor()->clear_exclusive();

        LOAD_NZCVT;

        if (current_pc != cpu->Reg[15]) {
            goto DISPATCH;
        }
    }

    FETCH_INST;
    GOTO_NEXT_INST;
}
SWP_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        swp_inst *inst_cream = (swp_inst *)inst_base->component;

        addr = RN;
        unsigned int value = cpu->ReadMemory32(addr);
        cpu->WriteMemory32(addr, RM);

        RD = value;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(swp_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
SWPB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        swp_inst *inst_cream = (swp_inst *)inst_base->component;
        addr = RN;
        unsigned int value = cpu->ReadMemory8(addr);
        cpu->WriteMemory8(addr, (RM & 0xFF));
        RD = value;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(swp_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
SXTAB_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        sxtab_inst *inst_cream = (sxtab_inst *)inst_base->component;

        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) & 0xff;

        // Sign extend for byte
        operand2 = (0x80 & operand2) ? (0xFFFFFF00 | operand2) : operand2;
        RD = RN + operand2;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(uxtab_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SXTAB16_INST:
SXTB16_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        sxtab_inst *const inst_cream = (sxtab_inst *)inst_base->component;

        const std::uint8_t rotation = inst_cream->rotate * 8;
        std::uint32_t rm_val = RM;
        std::uint32_t rn_val = RN;

        if (rotation)
            rm_val = ((rm_val << (32 - rotation)) | (rm_val >> rotation));

        // SXTB16
        if (inst_cream->Rn == 15) {
            std::uint32_t lo = (std::uint32_t)(std::int8_t)rm_val;
            std::uint32_t hi = (std::uint32_t)(std::int8_t)(rm_val >> 16);
            RD = (lo & 0xFFFF) | (hi << 16);
        }
        // SXTAB16
        else {
            std::uint32_t lo = rn_val + (std::uint32_t)(std::int8_t)(rm_val & 0xFF);
            std::uint32_t hi = (rn_val >> 16) + (std::uint32_t)(std::int8_t)((rm_val >> 16) & 0xFF);
            RD = (lo & 0xFFFF) | (hi << 16);
        }
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(sxtab_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

SXTAH_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        sxtah_inst *inst_cream = (sxtah_inst *)inst_base->component;

        unsigned int operand2 = ROTATE_RIGHT_32(RM, 8 * inst_cream->rotate) & 0xffff;
        // Sign extend for half
        operand2 = (0x8000 & operand2) ? (0xFFFF0000 | operand2) : operand2;
        RD = RN + operand2;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(sxtah_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

TEQ_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        teq_inst *const inst_cream = (teq_inst *)inst_base->component;

        std::uint32_t lop = RN;
        std::uint32_t rop = SHIFTER_OPERAND;

        if (inst_cream->Rn == 15)
            lop += cpu->GetInstructionSize() * 2;

        std::uint32_t result = lop ^ rop;

        UPDATE_NFLAG(result);
        UPDATE_ZFLAG(result);
        UPDATE_CFLAG_WITH_SC;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(teq_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
TST_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        tst_inst *const inst_cream = (tst_inst *)inst_base->component;

        std::uint32_t lop = RN;
        std::uint32_t rop = SHIFTER_OPERAND;

        if (inst_cream->Rn == 15)
            lop += cpu->GetInstructionSize() * 2;

        std::uint32_t result = lop & rop;

        UPDATE_NFLAG(result);
        UPDATE_ZFLAG(result);
        UPDATE_CFLAG_WITH_SC;
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(tst_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

UADD8_INST:
UADD16_INST:
UADDSUBX_INST:
USUB8_INST:
USUB16_INST:
USUBADDX_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *const inst_cream = (generic_arm_inst *)inst_base->component;

        const std::uint8_t op2 = inst_cream->op2;
        const std::uint32_t rm_val = RM;
        const std::uint32_t rn_val = RN;

        std::int32_t lo_result = 0;
        std::int32_t hi_result = 0;

        // UADD16
        if (op2 == 0x00) {
            lo_result = (rn_val & 0xFFFF) + (rm_val & 0xFFFF);
            hi_result = ((rn_val >> 16) & 0xFFFF) + ((rm_val >> 16) & 0xFFFF);

            if (lo_result & 0xFFFF0000) {
                cpu->Cpsr |= (1 << 16);
                cpu->Cpsr |= (1 << 17);
            } else {
                cpu->Cpsr &= ~(1 << 16);
                cpu->Cpsr &= ~(1 << 17);
            }

            if (hi_result & 0xFFFF0000) {
                cpu->Cpsr |= (1 << 18);
                cpu->Cpsr |= (1 << 19);
            } else {
                cpu->Cpsr &= ~(1 << 18);
                cpu->Cpsr &= ~(1 << 19);
            }
        }
        // UASX
        else if (op2 == 0x01) {
            lo_result = (rn_val & 0xFFFF) - ((rm_val >> 16) & 0xFFFF);
            hi_result = ((rn_val >> 16) & 0xFFFF) + (rm_val & 0xFFFF);

            if (lo_result >= 0) {
                cpu->Cpsr |= (1 << 16);
                cpu->Cpsr |= (1 << 17);
            } else {
                cpu->Cpsr &= ~(1 << 16);
                cpu->Cpsr &= ~(1 << 17);
            }

            if (hi_result >= 0x10000) {
                cpu->Cpsr |= (1 << 18);
                cpu->Cpsr |= (1 << 19);
            } else {
                cpu->Cpsr &= ~(1 << 18);
                cpu->Cpsr &= ~(1 << 19);
            }
        }
        // USAX
        else if (op2 == 0x02) {
            lo_result = (rn_val & 0xFFFF) + ((rm_val >> 16) & 0xFFFF);
            hi_result = ((rn_val >> 16) & 0xFFFF) - (rm_val & 0xFFFF);

            if (lo_result >= 0x10000) {
                cpu->Cpsr |= (1 << 16);
                cpu->Cpsr |= (1 << 17);
            } else {
                cpu->Cpsr &= ~(1 << 16);
                cpu->Cpsr &= ~(1 << 17);
            }

            if (hi_result >= 0) {
                cpu->Cpsr |= (1 << 18);
                cpu->Cpsr |= (1 << 19);
            } else {
                cpu->Cpsr &= ~(1 << 18);
                cpu->Cpsr &= ~(1 << 19);
            }
        }
        // USUB16
        else if (op2 == 0x03) {
            lo_result = (rn_val & 0xFFFF) - (rm_val & 0xFFFF);
            hi_result = ((rn_val >> 16) & 0xFFFF) - ((rm_val >> 16) & 0xFFFF);

            if ((lo_result & 0xFFFF0000) == 0) {
                cpu->Cpsr |= (1 << 16);
                cpu->Cpsr |= (1 << 17);
            } else {
                cpu->Cpsr &= ~(1 << 16);
                cpu->Cpsr &= ~(1 << 17);
            }

            if ((hi_result & 0xFFFF0000) == 0) {
                cpu->Cpsr |= (1 << 18);
                cpu->Cpsr |= (1 << 19);
            } else {
                cpu->Cpsr &= ~(1 << 18);
                cpu->Cpsr &= ~(1 << 19);
            }
        }
        // UADD8
        else if (op2 == 0x04) {
            std::int16_t sum1 = (rn_val & 0xFF) + (rm_val & 0xFF);
            std::int16_t sum2 = ((rn_val >> 8) & 0xFF) + ((rm_val >> 8) & 0xFF);
            std::int16_t sum3 = ((rn_val >> 16) & 0xFF) + ((rm_val >> 16) & 0xFF);
            std::int16_t sum4 = ((rn_val >> 24) & 0xFF) + ((rm_val >> 24) & 0xFF);

            if (sum1 >= 0x100)
                cpu->Cpsr |= (1 << 16);
            else
                cpu->Cpsr &= ~(1 << 16);

            if (sum2 >= 0x100)
                cpu->Cpsr |= (1 << 17);
            else
                cpu->Cpsr &= ~(1 << 17);

            if (sum3 >= 0x100)
                cpu->Cpsr |= (1 << 18);
            else
                cpu->Cpsr &= ~(1 << 18);

            if (sum4 >= 0x100)
                cpu->Cpsr |= (1 << 19);
            else
                cpu->Cpsr &= ~(1 << 19);

            lo_result = ((sum1 & 0xFF) | (sum2 & 0xFF) << 8);
            hi_result = ((sum3 & 0xFF) | (sum4 & 0xFF) << 8);
        }
        // USUB8
        else if (op2 == 0x07) {
            std::int16_t diff1 = (rn_val & 0xFF) - (rm_val & 0xFF);
            std::int16_t diff2 = ((rn_val >> 8) & 0xFF) - ((rm_val >> 8) & 0xFF);
            std::int16_t diff3 = ((rn_val >> 16) & 0xFF) - ((rm_val >> 16) & 0xFF);
            std::int16_t diff4 = ((rn_val >> 24) & 0xFF) - ((rm_val >> 24) & 0xFF);

            if (diff1 >= 0)
                cpu->Cpsr |= (1 << 16);
            else
                cpu->Cpsr &= ~(1 << 16);

            if (diff2 >= 0)
                cpu->Cpsr |= (1 << 17);
            else
                cpu->Cpsr &= ~(1 << 17);

            if (diff3 >= 0)
                cpu->Cpsr |= (1 << 18);
            else
                cpu->Cpsr &= ~(1 << 18);

            if (diff4 >= 0)
                cpu->Cpsr |= (1 << 19);
            else
                cpu->Cpsr &= ~(1 << 19);

            lo_result = (diff1 & 0xFF) | ((diff2 & 0xFF) << 8);
            hi_result = (diff3 & 0xFF) | ((diff4 & 0xFF) << 8);
        }

        RD = (lo_result & 0xFFFF) | ((hi_result & 0xFFFF) << 16);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

UHADD8_INST:
UHADD16_INST:
UHADDSUBX_INST:
UHSUBADDX_INST:
UHSUB8_INST:
UHSUB16_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *const inst_cream = (generic_arm_inst *)inst_base->component;
        const std::uint32_t rm_val = RM;
        const std::uint32_t rn_val = RN;
        const std::uint8_t op2 = inst_cream->op2;

        if (op2 == 0x00 || op2 == 0x01 || op2 == 0x02 || op2 == 0x03) {
            std::uint32_t lo_val = 0;
            std::uint32_t hi_val = 0;

            // UHADD16
            if (op2 == 0x00) {
                lo_val = (rn_val & 0xFFFF) + (rm_val & 0xFFFF);
                hi_val = ((rn_val >> 16) & 0xFFFF) + ((rm_val >> 16) & 0xFFFF);
            }
            // UHASX
            else if (op2 == 0x01) {
                lo_val = (rn_val & 0xFFFF) - ((rm_val >> 16) & 0xFFFF);
                hi_val = ((rn_val >> 16) & 0xFFFF) + (rm_val & 0xFFFF);
            }
            // UHSAX
            else if (op2 == 0x02) {
                lo_val = (rn_val & 0xFFFF) + ((rm_val >> 16) & 0xFFFF);
                hi_val = ((rn_val >> 16) & 0xFFFF) - (rm_val & 0xFFFF);
            }
            // UHSUB16
            else if (op2 == 0x03) {
                lo_val = (rn_val & 0xFFFF) - (rm_val & 0xFFFF);
                hi_val = ((rn_val >> 16) & 0xFFFF) - ((rm_val >> 16) & 0xFFFF);
            }

            lo_val >>= 1;
            hi_val >>= 1;

            RD = (lo_val & 0xFFFF) | ((hi_val & 0xFFFF) << 16);
        } else if (op2 == 0x04 || op2 == 0x07) {
            std::uint32_t sum1;
            std::uint32_t sum2;
            std::uint32_t sum3;
            std::uint32_t sum4;

            // UHADD8
            if (op2 == 0x04) {
                sum1 = (rn_val & 0xFF) + (rm_val & 0xFF);
                sum2 = ((rn_val >> 8) & 0xFF) + ((rm_val >> 8) & 0xFF);
                sum3 = ((rn_val >> 16) & 0xFF) + ((rm_val >> 16) & 0xFF);
                sum4 = ((rn_val >> 24) & 0xFF) + ((rm_val >> 24) & 0xFF);
            }
            // UHSUB8
            else {
                sum1 = (rn_val & 0xFF) - (rm_val & 0xFF);
                sum2 = ((rn_val >> 8) & 0xFF) - ((rm_val >> 8) & 0xFF);
                sum3 = ((rn_val >> 16) & 0xFF) - ((rm_val >> 16) & 0xFF);
                sum4 = ((rn_val >> 24) & 0xFF) - ((rm_val >> 24) & 0xFF);
            }

            sum1 >>= 1;
            sum2 >>= 1;
            sum3 >>= 1;
            sum4 >>= 1;

            RD = (sum1 & 0xFF) | ((sum2 & 0xFF) << 8) | ((sum3 & 0xFF) << 16) | ((sum4 & 0xFF) << 24);
        }
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

UMAAL_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        umaal_inst *const inst_cream = (umaal_inst *)inst_base->component;
        const std::uint64_t rm = RM;
        const std::uint64_t rn = RN;
        const std::uint64_t rd_lo = RDLO;
        const std::uint64_t rd_hi = RDHI;
        const std::uint64_t result = (rm * rn) + rd_lo + rd_hi;

        RDLO = (result & 0xFFFFFFFF);
        RDHI = ((result >> 32) & 0xFFFFFFFF);
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(umaal_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
UMLAL_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        umlal_inst *inst_cream = (umlal_inst *)inst_base->component;
        unsigned long long int rm = RM;
        unsigned long long int rs = RS;
        unsigned long long int rst = rm * rs;
        unsigned long long int add = ((unsigned long long)RDHI) << 32;
        add += RDLO;
        rst += add;
        RDLO = BITS(rst, 0, 31);
        RDHI = BITS(rst, 32, 63);

        if (inst_cream->S) {
            cpu->NFlag = BIT(RDHI, 31);
            cpu->ZFlag = (RDHI == 0 && RDLO == 0);
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(umlal_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
UMULL_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        umull_inst *inst_cream = (umull_inst *)inst_base->component;
        unsigned long long int rm = RM;
        unsigned long long int rs = RS;
        unsigned long long int rst = rm * rs;
        RDHI = BITS(rst, 32, 63);
        RDLO = BITS(rst, 0, 31);

        if (inst_cream->S) {
            cpu->NFlag = BIT(RDHI, 31);
            cpu->ZFlag = (RDHI == 0 && RDLO == 0);
        }
    }
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(umull_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}
B_2_THUMB : {
    b_2_thumb *inst_cream = (b_2_thumb *)inst_base->component;
    cpu->Reg[15] = cpu->Reg[15] + 4 + inst_cream->imm;
    INC_PC(sizeof(b_2_thumb));
    goto DISPATCH;
}
B_COND_THUMB : {
    b_cond_thumb *inst_cream = (b_cond_thumb *)inst_base->component;

    if (CondPassed(cpu, inst_cream->cond))
        cpu->Reg[15] = cpu->Reg[15] + 4 + inst_cream->imm;
    else
        cpu->Reg[15] += 2;

    INC_PC(sizeof(b_cond_thumb));
    goto DISPATCH;
}
BL_1_THUMB : {
    bl_1_thumb *inst_cream = (bl_1_thumb *)inst_base->component;
    cpu->Reg[14] = cpu->Reg[15] + 4 + inst_cream->imm;
    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(bl_1_thumb));
    FETCH_INST;
    GOTO_NEXT_INST;
}
BL_2_THUMB : {
    bl_2_thumb *inst_cream = (bl_2_thumb *)inst_base->component;
    int tmp = ((cpu->Reg[15] + 2) | 1);
    cpu->Reg[15] = (cpu->Reg[14] + inst_cream->imm);
    cpu->Reg[14] = tmp;
    INC_PC(sizeof(bl_2_thumb));
    goto DISPATCH;
}
BLX_1_THUMB : {
    // BLX 1 for armv5t and above
    std::uint32_t tmp = cpu->Reg[15];
    blx_1_thumb *inst_cream = (blx_1_thumb *)inst_base->component;
    cpu->Reg[15] = (cpu->Reg[14] + inst_cream->imm) & 0xFFFFFFFC;
    cpu->Reg[14] = ((tmp + 2) | 1);
    cpu->TFlag = 0;
    INC_PC(sizeof(blx_1_thumb));
    goto DISPATCH;
}

UQADD8_INST:
UQADD16_INST:
UQADDSUBX_INST:
UQSUB8_INST:
UQSUB16_INST:
UQSUBADDX_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *const inst_cream = (generic_arm_inst *)inst_base->component;

        const std::uint8_t op2 = inst_cream->op2;
        const std::uint32_t rm_val = RM;
        const std::uint32_t rn_val = RN;

        std::uint16_t lo_val = 0;
        std::uint16_t hi_val = 0;

        // UQADD16
        if (op2 == 0x00) {
            lo_val = ARMul_UnsignedSaturatedAdd16(rn_val & 0xFFFF, rm_val & 0xFFFF);
            hi_val = ARMul_UnsignedSaturatedAdd16((rn_val >> 16) & 0xFFFF, (rm_val >> 16) & 0xFFFF);
        }
        // UQASX
        else if (op2 == 0x01) {
            lo_val = ARMul_UnsignedSaturatedSub16(rn_val & 0xFFFF, (rm_val >> 16) & 0xFFFF);
            hi_val = ARMul_UnsignedSaturatedAdd16((rn_val >> 16) & 0xFFFF, rm_val & 0xFFFF);
        }
        // UQSAX
        else if (op2 == 0x02) {
            lo_val = ARMul_UnsignedSaturatedAdd16(rn_val & 0xFFFF, (rm_val >> 16) & 0xFFFF);
            hi_val = ARMul_UnsignedSaturatedSub16((rn_val >> 16) & 0xFFFF, rm_val & 0xFFFF);
        }
        // UQSUB16
        else if (op2 == 0x03) {
            lo_val = ARMul_UnsignedSaturatedSub16(rn_val & 0xFFFF, rm_val & 0xFFFF);
            hi_val = ARMul_UnsignedSaturatedSub16((rn_val >> 16) & 0xFFFF, (rm_val >> 16) & 0xFFFF);
        }
        // UQADD8
        else if (op2 == 0x04) {
            lo_val = ARMul_UnsignedSaturatedAdd8(rn_val, rm_val) | ARMul_UnsignedSaturatedAdd8(rn_val >> 8, rm_val >> 8) << 8;
            hi_val = ARMul_UnsignedSaturatedAdd8(rn_val >> 16, rm_val >> 16) | ARMul_UnsignedSaturatedAdd8(rn_val >> 24, rm_val >> 24) << 8;
        }
        // UQSUB8
        else {
            lo_val = ARMul_UnsignedSaturatedSub8(rn_val, rm_val) | ARMul_UnsignedSaturatedSub8(rn_val >> 8, rm_val >> 8) << 8;
            hi_val = ARMul_UnsignedSaturatedSub8(rn_val >> 16, rm_val >> 16) | ARMul_UnsignedSaturatedSub8(rn_val >> 24, rm_val >> 24) << 8;
        }

        RD = ((lo_val & 0xFFFF) | hi_val << 16);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

USAD8_INST:
USADA8_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        generic_arm_inst *inst_cream = (generic_arm_inst *)inst_base->component;

        const std::uint8_t ra_idx = inst_cream->Ra;
        const std::uint32_t rm_val = RM;
        const std::uint32_t rn_val = RN;

        const std::uint8_t diff1 = ARMul_UnsignedAbsoluteDifference(rn_val & 0xFF, rm_val & 0xFF);
        const std::uint8_t diff2 = ARMul_UnsignedAbsoluteDifference((rn_val >> 8) & 0xFF, (rm_val >> 8) & 0xFF);
        const std::uint8_t diff3 = ARMul_UnsignedAbsoluteDifference((rn_val >> 16) & 0xFF, (rm_val >> 16) & 0xFF);
        const std::uint8_t diff4 = ARMul_UnsignedAbsoluteDifference((rn_val >> 24) & 0xFF, (rm_val >> 24) & 0xFF);

        std::uint32_t finalDif = (diff1 + diff2 + diff3 + diff4);

        // Op is USADA8 if true.
        if (ra_idx != 15)
            finalDif += cpu->Reg[ra_idx];

        RD = finalDif;
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(generic_arm_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

USAT_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ssat_inst *const inst_cream = (ssat_inst *)inst_base->component;

        std::uint8_t shift_type = inst_cream->shift_type;
        std::uint8_t shift_amount = inst_cream->imm5;
        std::uint32_t rn_val = RN;

        // 32-bit ASR is encoded as an amount of 0.
        if (shift_type == 1 && shift_amount == 0)
            shift_amount = 31;

        if (shift_type == 0)
            rn_val <<= shift_amount;
        else if (shift_type == 1)
            rn_val = ((std::int32_t)rn_val >> shift_amount);

        bool saturated = false;
        rn_val = ARMul_UnsignedSatQ(rn_val, inst_cream->sat_imm, &saturated);

        if (saturated)
            cpu->Cpsr |= (1 << 27);

        RD = rn_val;
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ssat_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

USAT16_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        ssat_inst *const inst_cream = (ssat_inst *)inst_base->component;
        const std::uint8_t saturate_to = inst_cream->sat_imm;

        bool sat1 = false;
        bool sat2 = false;

        RD = (ARMul_UnsignedSatQ((std::int16_t)RN, saturate_to, &sat1) & 0xFFFF) | ARMul_UnsignedSatQ((std::int32_t)RN >> 16, saturate_to, &sat2) << 16;

        if (sat1 || sat2)
            cpu->Cpsr |= (1 << 27);
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(ssat_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

UXTAB16_INST:
UXTB16_INST : {
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        uxtab_inst *const inst_cream = (uxtab_inst *)inst_base->component;

        const std::uint8_t rn_idx = inst_cream->Rn;
        const std::uint32_t rm_val = RM;
        const std::uint32_t rotation = inst_cream->rotate * 8;
        const std::uint32_t rotated_rm = ((rm_val << (32 - rotation)) | (rm_val >> rotation));

        // UXTB16, otherwise UXTAB16
        if (rn_idx == 15) {
            RD = rotated_rm & 0x00FF00FF;
        } else {
            const std::uint32_t rn_val = RN;
            const std::uint8_t lo_rotated = (rotated_rm & 0xFF);
            const std::uint16_t lo_result = (rn_val & 0xFFFF) + (std::uint16_t)lo_rotated;
            const std::uint8_t hi_rotated = (rotated_rm >> 16) & 0xFF;
            const std::uint16_t hi_result = (rn_val >> 16) + (std::uint16_t)hi_rotated;

            RD = ((hi_result << 16) | (lo_result & 0xFFFF));
        }
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC(sizeof(uxtab_inst));
    FETCH_INST;
    GOTO_NEXT_INST;
}

WFE_INST : {
    // Stubbed, as WFE is a hint instruction.
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        LOG_TRACE(eka2l1::CPU_DYNCOM, "WFE executed.");
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC_STUB;
    FETCH_INST;
    GOTO_NEXT_INST;
}

WFI_INST : {
    // Stubbed, as WFI is a hint instruction.
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        LOG_TRACE(eka2l1::CPU_DYNCOM, "WFI executed.");
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC_STUB;
    FETCH_INST;
    GOTO_NEXT_INST;
}

YIELD_INST : {
    // Stubbed, as YIELD is a hint instruction.
    if (inst_base->cond == ConditionCode::AL || CondPassed(cpu, inst_base->cond)) {
        LOG_TRACE(eka2l1::CPU_DYNCOM, "YIELD executed.");
    }

    cpu->Reg[15] += cpu->GetInstructionSize();
    INC_PC_STUB;
    FETCH_INST;
    GOTO_NEXT_INST;
}

#define VFP_INTERPRETER_IMPL
#include <cpu/dyncom/vfp/vfpinstr.h>
#undef VFP_INTERPRETER_IMPL

UNDEFINED_ADDRESSING_MODE : {
    // GetAddressingOp() has no entry for this encoding, so the translator stored
    // a null addressing function. That means the guest is running an undefined
    // or unpredictable load/store form (data executed as code, for instance).
    // Report it as an undefined instruction: calling through the null pointer
    // would take the host process down instead of the offending guest thread.
    LOG_ERROR(eka2l1::CPU_DYNCOM, "Undefined load/store addressing mode (instruction 0x{:08X}) at 0x{:08X}",
        undef_inst, cpu->Reg[15]);

    SAVE_NZCVT;
    cpu->RaiseException(eka2l1::arm::exception_type_undefined_inst, cpu->Reg[15]);
    cpu->NumInstrsToExecute = 0;

    return num_instrs;
}

END : {
    SAVE_NZCVT;
    cpu->NumInstrsToExecute = 0;
    return num_instrs;
}
INIT_INST_LENGTH : {
    cpu->NumInstrsToExecute = 0;
    return num_instrs;
}
}
