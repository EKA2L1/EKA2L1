// Copyright (c) 2026 EKA2L1 Team. Licensed under GPLv2 or later.
#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <cpu/dyncom/arm_dyncom_thumb.h>

namespace eka2l1::arm::ekalynx::thumb {
    enum class code : std::uint8_t {
        other,
        lsl,
        lsr,
        asr,
        add,
        sub,
        add_i,
        sub_i,
        mov_i,
        cmp_i,
        bit_and,
        bit_xor,
        lsl_r,
        lsr_r,
        asr_r,
        adc,
        sbc,
        ror,
        tst,
        neg,
        cmp,
        cmn,
        bit_or,
        mul,
        bic,
        mvn,
        mov,
        add_high,
        cmp_high,
        adr,
        sp_add,
        sp_sub,
        ldr_pc,
        str,
        ldr,
        strb,
        ldrb,
        strh,
        ldrh,
        str_r,
        strh_r,
        strb_r,
        ldrsb_r,
        ldr_r,
        ldrh_r,
        ldrb_r,
        ldrsh_r,
        branch,
        branch_cond,
        call_hi,
        call_lo,
        multi_store,
        multi_load,
        cmp_branch,
        mov_lsl,
        exchange,
        call_exchange,
        call_arm,
        load_move,
        load_pair,
        shift_load_half,
        store_add,
        push_range,
        pop_range,
        call_pair,
        call_arm_pair,
        hint
    };

    struct op {
        code kind;
        std::uint8_t dst, lhs, rhs;
    };

    constexpr void fuse(op& first, op second) {
        if (first.kind == code::call_hi && (second.kind == code::call_lo || second.kind == code::call_arm)) {
            first.kind = second.kind == code::call_arm ? code::call_arm_pair : code::call_pair;
            return;
        }
        if ((first.kind == code::cmp || first.kind == code::cmp_high) && second.kind == code::branch_cond)
            first.kind = code::cmp_branch;
        else if (first.kind == code::lsl && !first.rhs && second.kind == code::lsl && second.rhs
            && first.dst == second.lhs && first.dst == second.dst)
            first.kind = code::mov_lsl;
        else if (first.kind == code::ldr && second.kind == code::lsl && !second.rhs)
            first.kind = code::load_move;
        else if (first.kind == code::ldr && second.kind == code::ldr)
            first.kind = code::load_pair;
        else if (first.kind == code::lsl && first.rhs && second.kind == code::ldrh_r)
            first.kind = code::shift_load_half;
        else if (first.kind == code::str && second.kind == code::add)
            first.kind = code::store_add;
    }

    constexpr op decode(std::uint16_t h) {
        const auto d = static_cast<std::uint8_t>(h & 7);
        const auto a = static_cast<std::uint8_t>((h >> 3) & 7);
        const auto b = static_cast<std::uint8_t>((h >> 6) & 7);
        const auto r = static_cast<std::uint8_t>((h >> 8) & 7);
        const auto byte = static_cast<std::uint8_t>(h);
        const auto offset = static_cast<std::uint8_t>((h >> 6) & 31);
        if (h < 0x1800) {
            constexpr code ops[] = { code::lsl, code::lsr, code::asr };
            return { ops[h >> 11], d, a, offset };
        }
        if (h < 0x2000) {
            constexpr code ops[] = { code::add, code::sub, code::add_i, code::sub_i };
            return { ops[(h >> 9) & 3], d, a, b };
        }
        if (h < 0x4000) {
            constexpr code ops[] = { code::mov_i, code::cmp_i, code::add_i, code::sub_i };
            return { ops[(h >> 11) & 3], r, r, byte };
        }
        if (h < 0x4400) {
            constexpr code ops[] = { code::bit_and, code::bit_xor, code::lsl_r, code::lsr_r, code::asr_r, code::adc,
                code::sbc, code::ror, code::tst, code::neg, code::cmp, code::cmn, code::bit_or, code::mul, code::bic,
                code::mvn };
            return { ops[(h >> 6) & 15], d, d, a };
        }
        if (h < 0x4700) {
            const auto rd = static_cast<std::uint8_t>(d | ((h >> 4) & 8));
            const auto rs = static_cast<std::uint8_t>((h >> 3) & 15);
            constexpr code ops[] = { code::add_high, code::cmp_high, code::mov };
            if (rs != 15 && rd != 15)
                return { ops[(h >> 8) & 3], rd, rd, rs };
        }
        if ((h & 0xff07) == 0x4700 && ((h >> 3) & 15) != 15)
            return { h & 128 ? code::call_exchange : code::exchange, 0, static_cast<std::uint8_t>((h >> 3) & 15), 0 };
        if ((h & 0xf801) == 0xe800)
            return { code::call_arm, 0, r, byte };
        if ((h >> 11) == 9)
            return { code::ldr_pc, r, 0, byte };
        if ((h >> 12) == 5) {
            constexpr code ops[] = { code::str_r, code::strh_r, code::strb_r, code::ldrsb_r, code::ldr_r, code::ldrh_r,
                code::ldrb_r, code::ldrsh_r };
            return { ops[(h >> 9) & 7], d, a, b };
        }
        if (h >= 0x6000 && h < 0x9000) {
            constexpr code ops[] = { code::str, code::ldr, code::strb, code::ldrb, code::strh, code::ldrh };
            return { ops[(h - 0x6000) >> 11], d, a, offset };
        }
        if ((h >> 12) == 9)
            return { h & 0x800 ? code::ldr : code::str, r, 13, byte };
        if ((h >> 12) == 10)
            return { code::adr, r, static_cast<std::uint8_t>(h & 0x800 ? 13 : 15), byte };
        if ((h >> 8) == 0xb0)
            return { h & 128 ? code::sp_sub : code::sp_add, 13, 13, static_cast<std::uint8_t>(h & 127) };
        if ((h & 0xf600) == 0xb400 && (h & 0x1ff)) {
            const bool pop = h & 0x800;
            const unsigned start = byte ? std::countr_zero(static_cast<unsigned>(byte)) : 0;
            const unsigned count = std::popcount(byte);
            if ((byte >> start) == ((1u << count) - 1))
                return { pop ? code::pop_range : code::push_range, static_cast<std::uint8_t>(h >> 8 & 1), 13,
                    static_cast<std::uint8_t>(count << 4 | start) };
            return { pop ? code::multi_load : code::multi_store, static_cast<std::uint8_t>(h >> 8 & 1), 13, byte };
        }
        if (IsThumbHint(h))
            return { code::hint, 0, 0, 0 };
        if ((h >> 12) == 12 && byte && !(byte & (1 << r)))
            return { h & 0x800 ? code::multi_load : code::multi_store, 0, r, byte };
        if ((h >> 11) == 30)
            return { code::call_hi, 0, r, byte };
        if ((h >> 11) == 31)
            return { code::call_lo, 0, r, byte };
        if (h >= 0xd000 && h < 0xde00)
            return { code::branch_cond, static_cast<std::uint8_t>((h >> 8) & 15), 0, byte };
        if ((h >> 11) == 28)
            return { code::branch, 0, r, byte };
        return {};
    }
}
