// Runs straight-line Thumb word loops with a signed, increasing counter.
// Only the final CMP feeds a condition, so intermediate NZCV updates can be skipped.
struct thumb_counted_loop {
    enum operation : std::uint8_t {
        move, constant, add, add_imm, sub, mul, bit_and, bit_or, bit_xor,
        lsl, lsr, asr, load, store
    };

    struct step {
        operation op;
        std::uint8_t dst, lhs, rhs;
    };

    struct access {
        std::uint32_t offset;
        std::uint8_t base;
        bool write;
    };

    static constexpr std::uint8_t pc_relative = 15;
    static constexpr std::size_t max_steps = 40;
    step steps[max_steps];
    access accesses[8];
    std::uint8_t strides[15];
    std::uint8_t step_count, access_count, counter, limit, body_len;
};

static bool analyze_thumb_counted_loop(ARMul_State *cpu, std::uint32_t start, thumb_counted_loop &out) {
    using loop = thumb_counted_loop;
    out = {};
    std::uint16_t affine = 0x7fff;
    std::uint16_t address_bases = 0;
    bool stores = false;
    auto emit = [&](loop::operation op, int dst, int lhs, int rhs) {
        if (op != loop::store && !(op == loop::add_imm && dst == lhs)) {
            affine &= ~(1u << dst);
        }
        out.steps[out.step_count++] = { op, static_cast<std::uint8_t>(dst),
            static_cast<std::uint8_t>(lhs), static_cast<std::uint8_t>(rhs) };
    };
    auto advance = [&](int reg, unsigned amount) {
        if ((affine & (1u << reg)) && amount <= 255u - out.strides[reg]) {
            out.strides[reg] += amount;
        } else {
            affine &= ~(1u << reg);
        }
        emit(loop::add_imm, reg, reg, amount);
    };
    auto memory = [&](bool write, int reg, int base, std::uint32_t offset) {
        if (out.access_count == std::size(out.accesses))
            return false;
        if (base != loop::pc_relative) {
            if (!(affine & (1u << base)))
                return false;
            address_bases |= 1u << base;
            offset += out.strides[base];
        }
        const auto slot = out.access_count++;
        out.accesses[slot] = { offset, static_cast<std::uint8_t>(base), write };
        emit(write ? loop::store : loop::load, reg, slot, 0);
        stores |= write;
        return true;
    };

    // The regular translator also terminates blocks at the 4 KiB boundary.
    const std::uint32_t page = start & ~0xfffu;
    auto fetch = [&](std::uint32_t pc) {
        return static_cast<std::uint16_t>(cpu->ReadCode(pc & ~3u) >> ((pc & 2) * 8));
    };
    for (std::uint32_t pc = start; (pc & ~0xfffu) == page && out.step_count + 2 <= loop::max_steps; pc += 2) {
        const auto hw = fetch(pc);
        const int rd = hw & 7;
        const int rn = (hw >> 3) & 7;
        if ((hw & 0xffc0) == 0x4280 || (hw & 0xff00) == 0x4500) { // CMP
            if (((pc + 2) & ~0xfffu) != page)
                return false;
            const auto branch = fetch(pc + 2);
            const auto target = pc + 6 + static_cast<std::int8_t>(branch & 0xff) * 2;
            const int counter = rd | ((hw & 0xff00) == 0x4500 ? (hw >> 4) & 8 : 0);
            const int limit = (hw & 0xff00) == 0x4500 ? (hw >> 3) & 15 : rn;
            if ((branch & 0xff00) != 0xdb00 || target != start || counter >= 15 || limit >= 15
                || counter == limit || !stores || (out.step_count == 0)
                || ((address_bases | (1u << counter) | (1u << limit)) & ~affine)
                || out.strides[counter] != 1 || out.strides[limit] != 0)
                return false;
            out.counter = counter;
            out.limit = limit;
            out.body_len = (pc - start) / 2 + 2;
            return true;
        }
        if ((hw & 0xe000) == 0) { // immediate shifts and add/subtract
            const unsigned kind = (hw >> 11) & 3;
            const unsigned operand = (hw >> 6) & 7;
            if (kind < 3) {
                const unsigned shift = (hw >> 6) & 31;
                emit(kind == 0 ? loop::lsl : kind == 1 ? loop::lsr : loop::asr,
                    rd, rn, shift == 0 && kind != 0 ? 32 : shift);
            } else if ((hw & 0x0600) == 0) {
                emit(loop::add, rd, rn, operand);
            } else if ((hw & 0x0600) == 0x0200) {
                emit(loop::sub, rd, rn, operand);
            } else if ((hw & 0x0600) == 0x0400 && rd == rn) {
                advance(rd, operand);
            } else {
                return false;
            }
        } else if ((hw & 0xf800) == 0x2000) {
            emit(loop::constant, (hw >> 8) & 7, 0, hw & 255);
        } else if ((hw & 0xf800) == 0x3000) {
            advance((hw >> 8) & 7, hw & 255);
        } else if ((hw & 0xfc00) == 0x4000) {
            switch ((hw >> 6) & 15) {
            case 0: emit(loop::bit_and, rd, rd, rn); break;
            case 1: emit(loop::bit_xor, rd, rd, rn); break;
            case 12: emit(loop::bit_or, rd, rd, rn); break;
            case 13: emit(loop::mul, rd, rd, rn); break;
            default: return false;
            }
        } else if ((hw & 0xff00) == 0x4400 || (hw & 0xff00) == 0x4600) {
            const int dst = rd | ((hw >> 4) & 8);
            const int src = (hw >> 3) & 15;
            if (dst == 13 || dst == 15 || src == 15)
                return false;
            emit((hw & 0x0200) ? loop::move : loop::add, dst, dst, src);
        } else if ((hw & 0xf800) == 0x4800) {
            if (!memory(false, (hw >> 8) & 7, loop::pc_relative, ((pc + 4) & ~3u) + (hw & 255) * 4))
                return false;
        } else if ((hw & 0xf000) == 0x6000) {
            if (!memory(!(hw & 0x0800), rd, rn, ((hw >> 6) & 31) * 4))
                return false;
        } else if ((hw & 0xf000) == 0x9000) {
            if (!memory(!(hw & 0x0800), (hw >> 8) & 7, 13, (hw & 255) * 4))
                return false;
        } else if ((hw & 0xf800) == 0xc000) {
            const unsigned list = hw & 255;
            const int base = (hw >> 8) & 7;
            if (!std::has_single_bit(list) || (list & (1u << base))
                || !memory(true, std::countr_zero(list), base, 0))
                return false;
            advance(base, 4);
        } else {
            return false;
        }
    }
    return false;
}

// Resolve each stream over the same page before entering the loop. A failed
// check leaves the iteration untouched for the interpreter and its fault path.
static std::uint32_t run_thumb_counted_loop(ARMul_State *cpu, const thumb_counted_loop &loop,
    std::uint64_t budget) {
    using op = thumb_counted_loop;
    const unsigned body = loop.body_len;
    auto &reg = cpu->Reg;
    const auto remaining = static_cast<std::int64_t>(static_cast<std::int32_t>(reg[loop.limit]))
        - static_cast<std::int32_t>(reg[loop.counter]);
    if (remaining <= 1 || budget < 2u * body || cpu->InBigEndianMode()
        || (!cpu->NirqSig && !(cpu->Cpsr & 0x80)))
        return 0;

    auto count = static_cast<std::uint32_t>(std::min<std::uint64_t>(budget / body, remaining - 1));
    auto &tlb = *cpu->mem_cache_;
    const auto page_mask = static_cast<std::uint32_t>(tlb.page_mask);
    std::uint8_t *pointers[std::size(loop.accesses)];
    unsigned strides[std::size(loop.accesses)];
    for (unsigned j = 0; j < loop.access_count; ++j) {
        const auto &access = loop.accesses[j];
        const auto base = access.base == op::pc_relative ? 0 : reg[access.base];
        const auto addr = base + access.offset;
        const auto off = addr & page_mask;
        const auto page = addr - off;
        const auto &entry = tlb.entries[(addr >> tlb.page_bits) & eka2l1::arm::r12l1::TLB_ENTRY_MASK];
        if ((addr & 3) || off > page_mask - 3 || !entry.host_base
            || (access.write ? entry.write_addr : entry.read_addr) != page)
            return 0;
        pointers[j] = entry.host_base + off;
        const auto stride = access.base == op::pc_relative ? 0 : loop.strides[access.base];
        strides[j] = stride;
        if (stride) {
            if (stride & 3)
                return 0;
            count = std::min(count, (page_mask - off - 3) / stride + 1);
        }
    }
    if (count < 2)
        return 0;

    for (unsigned i = 0; i < count; ++i) {
        for (unsigned j = 0; j < loop.step_count; ++j) {
            const auto &step = loop.steps[j];
            const auto lhs = step.lhs, rhs = step.rhs;
            auto &dst = reg[step.dst];
            switch (step.op) {
            case op::move: dst = reg[rhs]; break;
            case op::constant: dst = rhs; break;
            case op::add: dst = reg[lhs] + reg[rhs]; break;
            case op::add_imm: dst = reg[lhs] + rhs; break;
            case op::sub: dst = reg[lhs] - reg[rhs]; break;
            case op::mul: dst = reg[lhs] * reg[rhs]; break;
            case op::bit_and: dst = reg[lhs] & reg[rhs]; break;
            case op::bit_or: dst = reg[lhs] | reg[rhs]; break;
            case op::bit_xor: dst = reg[lhs] ^ reg[rhs]; break;
            case op::lsl: dst = reg[lhs] << rhs; break;
            case op::lsr: dst = rhs == 32 ? 0 : reg[lhs] >> rhs; break;
            case op::asr: dst = static_cast<std::int32_t>(reg[lhs]) >> std::min<unsigned>(rhs, 31); break;
            case op::load: std::memcpy(&dst, pointers[lhs] + i * strides[lhs], 4); break;
            case op::store: std::memcpy(pointers[lhs] + i * strides[lhs], &dst, 4); break;
            }
        }
    }
#if defined(EKA2L1_DYNCOM_DIFFTEST)
    g_loop_accel_bulk_iterations += count;
#endif
    bool carry, overflow;
    const auto result = AddWithCarry(reg[loop.counter], ~reg[loop.limit], 1, &carry, &overflow);
    cpu->NFlag = result >> 31;
    cpu->ZFlag = result == 0;
    cpu->CFlag = carry;
    cpu->VFlag = overflow;
    return count * body;
}
