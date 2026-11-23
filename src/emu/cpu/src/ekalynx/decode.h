// Copyright (c) 2026 EKA2L1 Team. Licensed under GPLv2 or later.
#pragma once
#include <bit>
#include <cstdint>
#include <cpu/dyncom/arm_dyncom_thumb.h>

namespace eka2l1::arm::ekalynx {
    enum class opcode : std::uint8_t {
        empty,
        undefined,
        data,
        transfer,
        multiple,
        branch,
        exchange,
        multiply,
        half,
        thumb_branch,
        thumb_prefix,
        thumb_call,
        supervisor,
        exclusive_read,
        exclusive_write,
        clear_exclusive,
        hint,
        read_status,
        write_flags,
        long_multiply,
        multiply_accumulate_words,
        multiply_high,
        vfp_move,
        vfp_status,
        vfp_transfer,
        vfp_move_double,
        vfp_int_to_double,
        vfp_arithmetic,
        vfp_multiple,
        count_leading_zeros,
        signed_multiply,
        parallel_arithmetic,
        select_bytes,
        pack_halves,
        saturate,
        sum_byte_distances,
        saturating_add_sub,
        thread_id,
        reverse_bytes,
        extend,
        double_transfer,
        breakpoint,
        set_endian,
        dual_multiply,
        swap,
        feature_register,
        barrier
    };

    struct instruction {
        std::uint32_t bits;
        opcode op;
        std::uint8_t condition;
    };

    constexpr bool vfp_arithmetic(std::uint32_t bits) {
        if ((bits & 0x0f000e10) != 0x0e000a00)
            return false;
        const auto operation = (bits >> 20) & 11;
        if (operation < 4 || (operation == 8 && !(bits & 64)))
            return true;
        if (operation != 11 || !(bits & 64))
            return false;
        const auto kind = ((bits >> 15) & 30) | ((bits >> 7) & 1);
        return kind <= 3 || kind == 8 || kind == 9 || ((kind == 10 || kind == 11) && !(bits & 47)) || kind == 15
            || kind == 16 || kind == 17 || (kind >= 24 && kind <= 27);
    }

    constexpr unsigned vfp_payload_words(std::uint32_t bits) {
        // Legacy double lists include a trailing format word in the address span.
        return (bits & 255) & ((bits & 256) ? ~1u : ~0u);
    }

    constexpr bool vfp_multiple(std::uint32_t bits) {
        if ((bits & 0x0e000e00) != 0x0c000a00 || ((bits >> 16) & 15) == 15)
            return false;
        const bool up = bits & (1u << 23), pre = bits & (1u << 24), writeback = bits & (1u << 21);
        if (pre == up || (pre && !writeback))
            return false;
        const bool wide = bits & 256;
        const auto high = (bits >> 22) & 1;
        const auto first = 2 * ((bits >> 12) & 15) + (wide ? 32 * high : high);
        const auto words = vfp_payload_words(bits);
        return words && (!wide || words <= 32) && first + words <= (wide ? 64 : 32);
    }

    constexpr unsigned vfp_move_word(std::uint32_t bits) {
        const auto base = 2 * ((bits >> 16) & 15);
        return base + (bits & 256 ? ((bits >> 2) & 32) + ((bits >> 21) & 1) : (bits >> 7) & 1);
    }

    constexpr unsigned vfp_move_pair(std::uint32_t bits) {
        return 2 * (bits & 15) + (bits & 256 ? bits & 32 : (bits >> 5) & 1);
    }

    constexpr std::uint32_t user_status_mask(std::uint32_t bits) {
        return (bits & 0x80000 ? 0xf8000000u : 0u) | (bits & 0x40000 ? 0xf0000u : 0u)
            | (bits & 0x20000 ? 0x200u : 0u);
    }

    inline instruction decode(std::uint32_t bits, bool thumb) {
        if (thumb) {
            const auto half = static_cast<std::uint16_t>(bits);
            const auto group = half >> 11;
            if ((half & 0xff00) == 0xbe00)
                return { half, opcode::breakpoint, 14 };
            if ((half >> 12) == 13 && ((half >> 8) & 15) < 14)
                return { half, opcode::thumb_branch, static_cast<std::uint8_t>((half >> 8) & 15) };
            if (group == 28)
                return { half, opcode::thumb_branch, 14 };
            if (group == 30)
                return { half, opcode::thumb_prefix, 14 };
            if (group == 31 || (group == 29 && !(half & 1)))
                return { half, opcode::thumb_call, 14 };
            if ((half & 0xff87) == 0x4780)
                return { 0xe12fff30u | ((half >> 3) & 15), opcode::exchange, 14 };
            std::uint32_t size;
            const auto status = TranslateThumbInstruction(0, half, &bits, &size);
            if (status == ThumbDecodeStatus::UNDEFINED || status == ThumbDecodeStatus::BRANCH)
                return { half, opcode::undefined, 14 };
        }
        const auto cond = static_cast<std::uint8_t>(bits >> 28);
        const auto rn = (bits >> 16) & 15, rd = (bits >> 12) & 15, rm = bits & 15;
        auto op = opcode::undefined;
        if (cond != 15) {
            if ((bits & 0x0ffffff0) == 0x012fff10 || (bits & 0x0ffffff0) == 0x012fff30
                || (bits & 0x0ffffff0) == 0x012fff20)
                op = opcode::exchange;
            else if ((bits & 0x0ff000f0) == 0x01200070)
                op = opcode::breakpoint;
            else if ((bits & 0x0fff0ff0) == 0x06bf0f30 || (bits & 0x0fbf0ff0) == 0x06bf0fb0)
                op = opcode::reverse_bytes;
            else if ((bits & 0x0f8003f0) == 0x06800070 && ((bits >> 20) & 3) != 1)
                op = opcode::extend;
            else if ((bits & 0x0e1000d0) == 0x000000d0 && (bits & 0x01200000) != 0x00200000 && !((bits >> 12) & 1)
                && rd < 14 && ((bits & (1u << 22)) || (!(bits & 0xf00) && rm != 15)))
                op = opcode::double_transfer;
            else if ((bits & 0x0fff0ff0) == 0x016f0f10)
                op = opcode::count_leading_zeros;
            else if ((bits & 0x0f900ff0) == 0x01000050)
                op = opcode::saturating_add_sub;
            else if ((bits & 0x0ff00030) == 0x06800010)
                op = opcode::pack_halves;
            else if ((bits & 0x0fa00030) == 0x06a00010 || (bits & 0x0fb00ff0) == 0x06a00f30)
                op = opcode::saturate;
            else if ((bits & 0x0ff000f0) == 0x07800010)
                op = opcode::sum_byte_distances;
            else if ((bits & 0x0ff00ff0) == 0x06800fb0)
                op = opcode::select_bytes;
            else if ((bits & 0x0f800f10) == 0x06000f10 && ((bits >> 20) & 3)
                && ((bits >> 5 & 7) < 5 || (bits >> 5 & 7) == 7))
                op = opcode::parallel_arithmetic;
            else if ((bits & 0x0f900090) == 0x01000080)
                op = opcode::signed_multiply;
            else if ((bits & 0x0fb00090) == 0x07000010 && rm != 15 && ((bits >> 8) & 15) != 15
                && rn != 15 && (!(bits & (1u << 22))
                    || (rd != 15 && rd != rn)))
                op = opcode::dual_multiply;
            else if ((bits & 0x0fb00ff0) == 0x01000090 && rm != 15
                && rd != 15 && rn != 15 && rn != rd && rn != rm)
                op = opcode::swap;
            else if ((bits & 0x0ffffff0) == 0x0320f000 && rm <= 4)
                op = opcode::hint;
            else if ((bits & 0x0fff0fff) == 0x0e1d0f70 && rd != 15)
                op = opcode::feature_register;
            else if ((bits & 0x0fff0fff) == 0x0e070fba || (bits & 0x0fff0fff) == 0x0e070f9a
                || (bits & 0x0fff0fff) == 0x0e070f95)
                op = opcode::barrier;
            else if ((bits & 0x0fef0fff) == 0x0e0d0f50 && rd != 15)
                op = opcode::thread_id;
            else if ((bits & 0x0fe00ed0) == 0x0c400a10 && ((bits & 256) || (bits & 47) != 47)
                && rd != 15 && rn != 15 && (!(bits & (1u << 20)) || rn != rd))
                op = opcode::vfp_move_double;
            else if ((bits & 0x0fbf0f50) == 0x0eb80b40)
                op = opcode::vfp_int_to_double;
            else if (vfp_arithmetic(bits))
                op = opcode::vfp_arithmetic;
            else if (vfp_multiple(bits))
                op = opcode::vfp_multiple;
            else if ((bits & 0x0fe00f7f) == 0x0e000a10 && rd != 15)
                op = opcode::vfp_move;
            else if ((bits & 0x0fc00f7f) == 0x0e000b10 && rd != 15)
                op = opcode::vfp_move;
            else if ((bits & 0x0ff00fff) == 0x0ef00a10 && rd != 15
                && (rn == 0 || rn == 6 || rn == 7))
                op = opcode::feature_register;
            else if ((bits & 0x0fef0fff) == 0x0ee10a10 && ((bits & (1u << 20)) || rd != 15))
                op = opcode::vfp_status;
            else if ((bits & 0x0f200e00) == 0x0d000a00)
                op = opcode::vfp_transfer;
            else if ((bits & 0x0f900fff) == 0x01900f9f && rd != 15)
                op = opcode::exclusive_read;
            else if ((bits & 0x0f900ff0) == 0x01800f90 && rm != 15)
                op = opcode::exclusive_write;
            else if ((bits & 0x0fbf0fff) == 0x010f0000 && !(bits & 0x00400000))
                op = opcode::read_status;
            else if ((((bits & 0x0ff0fff0) == 0x0120f000 && rm != 15)
                    || (bits & 0x0ff0f000) == 0x0320f000) && (bits & 0xf0000))
                op = opcode::write_flags;
            else if ((bits & 0x0ff000f0) == 0x00400090)
                op = opcode::multiply_accumulate_words;
            else if ((bits & 0x0ff000d0) == 0x07500010
                || ((bits & 0x0ff000d0) == 0x075000d0 && rd != 15))
                op = opcode::multiply_high;
            else if ((bits & 0x0f8000f0) == 0x00800090)
                op = opcode::long_multiply;
            else if ((bits & 0x0fc000f0) == 0x00000090)
                op = opcode::multiply;
            else if ((bits & 0x0e000090) == 0x00000090 && (bits & 0x60)
                && ((bits & 0x01200000) != 0x00200000 || (rn != rd
                    && rn != 15 && rd != 15
                    && ((bits & (1u << 22)) || (!(bits & 0xf00) && rm != 15))))
                && ((bits & (1u << 20)) || (bits & 0x60) == 0x20))
                op = opcode::half;
            else if ((bits & 0x0c000000) == 0 && ((bits >> 23 & 3) != 2 || (bits >> 20 & 1))
                && ((bits >> 25 & 1) || (bits & 0x90) != 0x90))
                op = opcode::data;
            else if ((bits & 0x0c000000) == 0x04000000 && (bits & 0x02000010) != 0x02000010)
                op = opcode::transfer;
            else if ((bits & 0x0e400000) == 0x08000000 && (bits & 0xffff))
                op = opcode::multiple;
            else if ((bits & 0x0f000000) == 0x0f000000)
                op = opcode::supervisor;
        }
        if ((bits & 0x0e000000) == 0x0a000000)
            op = opcode::branch;
        if ((bits & 0xfd70f000) == 0xf550f000 && (!(bits & (1u << 25)) || !(bits & 16)))
            op = opcode::hint;
        if ((bits & 0xfff1fe20) == 0xf1000000 && ((bits & 0x20000) || !(bits & 31))
            && ((bits & 0x80000) ? (bits & 0x1c0) != 0 : (bits & 0x601c0) == 0x20000))
            op = opcode::hint; // CPS has no effect in User mode.
        if ((bits & ~0x200u) == 0xf1010000)
            op = opcode::set_endian;
        if (bits == 0xf57ff01f)
            op = opcode::clear_exclusive;
        if (op == opcode::data && (bits & 0x0010f000) == 0x0010f000)
            op = opcode::undefined;
        return { bits, op, cond };
    }

#define EKALYNX_ARM_CODES(X) \
    X(other) \
    X(add_i) \
    X(add_lsl) \
    X(sub_i) \
    X(mul) \
    X(mla) \
    X(smull) \
    X(sub_lsl) \
    X(adc_r) \
    X(rsbs_lsr) \
    X(mov_i) \
    X(mov_lsl) \
    X(mov_lsr) \
    X(mov_asr) \
    X(cmp_r) \
    X(cmp_i) \
    X(branch) \
    X(ldr) \
    X(str) \
    X(ldrb) \
    X(strb) \
    X(ldrh) \
    X(ldrsh) \
    X(strh) \
    X(ldr_index) \
    X(str_index) \
    X(ldrb_post) \
    X(strb_post) \
    X(ldr_post) \
    X(str_post) \
    X(ldm) \
    X(stm) \
    X(call) \
    X(bx) \
    X(blx) \
    X(ldr_pc_literal) \
    X(ldr_pc_table) \
    X(and_i) \
    X(tst_i) \
    X(ands_i) \
    X(cmp_branch) \
    X(str_pair) \
    X(vmov_from) \
    X(vmov_to) \
    X(vmov_pair_from) \
    X(vmov_pair_to) \
    X(widen_integer) \
    X(data) \
    X(vfp_load) \
    X(vfp_store) \
    X(vfp_math) \
    X(write_flags) \
    X(word_loop_entry) \
    X(hint) \
    X(dual_multiply) \
    X(orr_lsl) \
    X(subs_i) \
    X(page_end)
    enum class arm_code : std::uint8_t {
#define EKALYNX_ARM_CODE_ENUM(name) name,
        EKALYNX_ARM_CODES(EKALYNX_ARM_CODE_ENUM)
#undef EKALYNX_ARM_CODE_ENUM
    };
    struct alignas(16) arm_op {
        arm_code code {};
        opcode generic {};
        std::uint8_t d {}, a {}, b {}, shift {}, flags {}, cond {};
        std::uint32_t value {};
    };
    inline arm_op specialize_arm(instruction ins) {
        const auto bits = ins.bits;
        arm_op op;
        op.a = (bits >> 16) & 15;
        op.b = bits & 15;
        op.d = (bits >> 12) & 15;
        op.shift = (bits >> 7) & 31;
        op.cond = ins.condition;
        switch (ins.op) {
        case opcode::hint:
            op.code = arm_code::hint;
            return op;
        case opcode::dual_multiply:
            op.code = arm_code::dual_multiply;
            op.value = bits;
            return op;
        case opcode::vfp_transfer:
        case opcode::vfp_multiple: {
            const bool multiple = ins.op == opcode::vfp_multiple;
            const bool wide = bits & 256, up = bits & (1u << 23);
            const auto high = (bits >> 22) & 1;
            op.code = bits & (1u << 20) ? arm_code::vfp_load : arm_code::vfp_store;
            op.b = 2 * op.d + (wide ? 32 * high : high);
            op.shift = multiple ? vfp_payload_words(bits) : (wide ? 2 : 1);
            op.value = (bits & 255) * (up ? 4u : -4u);
            op.flags = (multiple && up ? 1 : 0) | (multiple && (bits & (1u << 21)) ? 2 : 0);
            return op;
        }
        case opcode::vfp_arithmetic:
            op.code = arm_code::vfp_math;
            op.value = bits;
            return op;
        case opcode::write_flags:
            // The page runner assumes little-endian data until it returns to dispatch.
            if (bits & ((1u << 25) | (1u << 17)))
                break;
            op.value = user_status_mask(bits);
            op.code = arm_code::write_flags;
            return op;
        case opcode::vfp_move:
            op.code = bits & (1u << 20) ? arm_code::vmov_from : arm_code::vmov_to;
            op.value = vfp_move_word(bits);
            return op;
        case opcode::vfp_move_double:
            op.code = bits & (1u << 20) ? arm_code::vmov_pair_from : arm_code::vmov_pair_to;
            op.value = vfp_move_pair(bits);
            return op;
        case opcode::vfp_int_to_double:
            op.code = arm_code::widen_integer;
            op.b = 2 * op.b + ((bits >> 5) & 1);
            op.value = 2 * op.d + ((bits & (1u << 22)) ? 32 : 0);
            op.flags = bits & 128;
            return op;
        default:
            break;
        }
        if (ins.op == opcode::exchange && op.b != 15) {
            op.code = (bits & 48) == 48 ? arm_code::blx : arm_code::bx;
            return op;
        }
        if ((bits & 0x0f7ff000) == 0x051ff000 && ins.condition != 15) {
            op.code = arm_code::ldr_pc_literal;
            op.value = bits & (1u << 23) ? 8 + (bits & 4095) : 8 - (bits & 4095);
            return op;
        }
        if ((bits & 0x0ffff070) == 0x079ff000 && op.b != 15 && ins.condition != 15) {
            op.code = arm_code::ldr_pc_table;
            return op;
        }
        if (ins.op == opcode::branch && ins.condition != 15) {
            op.code = bits & (1u << 24) ? arm_code::call : arm_code::branch;
            op.value = 2 + (static_cast<std::int32_t>(bits << 8) >> 8);
            return op;
        }
        if (ins.condition == 15 || op.a == 15 || op.d == 15)
            return op;
        const bool load = bits & (1 << 20);
        if (ins.op == opcode::multiply && !load && op.b != 15 && ((bits >> 8) & 15) != 15
            && ((bits & (1u << 21)) || op.d == 0)) {
            op.code = (bits & (1u << 21)) ? arm_code::mla : arm_code::mul;
            op.value = op.d;
            op.d = op.a;
            op.a = (bits >> 8) & 15;
            return op;
        }
        if (ins.op == opcode::long_multiply && (bits & 0x00700000) == 0x00400000
            && op.d != op.a && op.b != 15 && ((bits >> 8) & 15) != 15) {
            op.code = arm_code::smull;
            op.value = (bits >> 8) & 15;
            return op;
        }
        if (ins.op == opcode::half && (bits & (1u << 22)) && !(bits & (1u << 21))) {
            const bool post = !(bits & (1u << 24));
            const auto kind = (bits >> 5) & 3;
            if (kind != 2 && (!post || op.a != op.d)) {
                op.code = !load ? arm_code::strh : kind == 1 ? arm_code::ldrh : arm_code::ldrsh;
                const auto offset = ((bits >> 4) & 0xf0) | (bits & 15);
                op.value = bits & (1u << 23) ? offset : -offset;
                op.flags = post;
            }
            return op;
        }
        if (ins.op == opcode::data) {
            const unsigned kind = (bits >> 21) & 15;
            const bool imm = bits & (1 << 25);
            if (!imm && op.b != 15 && !(bits & 16) && op.shift) {
                const auto shift_type = (bits >> 5) & 3;
                if (!load && kind == 13 && (shift_type == 1 || shift_type == 2)) {
                    op.code = shift_type == 1 ? arm_code::mov_lsr : arm_code::mov_asr;
                    return op;
                }
                if (load && kind == 3 && shift_type == 1) {
                    op.code = arm_code::rsbs_lsr;
                    return op;
                }
            }
            if (!imm && ((bits & 0x70) || op.b == 15))
                return op;
            if (kind == 10 && (imm || !op.shift))
                op.code = imm ? arm_code::cmp_i : arm_code::cmp_r;
            else if (imm && (kind == 0 || (kind == 8 && load))) {
                op.code = kind == 8 ? arm_code::tst_i : load ? arm_code::ands_i : arm_code::and_i;
                op.flags = (bits >> 8) & 15;
            } else if (!load && kind == 4)
                op.code = imm ? arm_code::add_i : arm_code::add_lsl;
            else if (!load && kind == 2)
                op.code = imm ? arm_code::sub_i : arm_code::sub_lsl;
            else if (!load && kind == 5 && !imm && !op.shift)
                op.code = arm_code::adc_r;
            else if (!load && kind == 13)
                op.code = imm ? arm_code::mov_i : arm_code::mov_lsl;
            else if (!load && kind == 12 && !imm)
                op.code = arm_code::orr_lsl;
            else if (load && kind == 2 && imm)
                op.code = arm_code::subs_i;
            op.value = std::rotr(bits & 255u, static_cast<int>((bits >> 7) & 30));
            if (!load && imm && (kind == 14 || kind == 15)) {
                op.code = kind == 14 ? arm_code::and_i : arm_code::mov_i;
                op.value = ~op.value;
            }
        } else if (ins.op == opcode::multiple && (!(bits & (1u << 21)) || !(bits & (1u << op.a)))) {
            op.code = load ? arm_code::ldm : arm_code::stm;
            op.value = bits & 65535;
            op.shift = std::popcount(op.value);
            const bool up = bits & (1u << 23), pre = bits & (1u << 24);
            op.b = (bits & (1u << 21)) ? static_cast<std::uint8_t>(up ? 4 * op.shift : -4 * op.shift) : 0;
            op.flags = up ? pre * 4 : (op.shift - !pre) * 4;
            if (!up)
                op.flags = -op.flags;
        } else if (ins.op == opcode::transfer) {
            bool imm = !(bits & (1 << 25)), pre = bits & (1 << 24), byte = bits & (1 << 22);
            bool up = bits & (1 << 23), wb = bits & (1 << 21);
            op.value = up ? bits & 4095 : -(bits & 4095);
            if (imm && pre && !wb)
                op.code = byte ? (load ? arm_code::ldrb : arm_code::strb) : (load ? arm_code::ldr : arm_code::str);
            else if (imm && !pre && !wb && op.a != op.d)
                op.code = byte ? (load ? arm_code::ldrb_post : arm_code::strb_post)
                               : (load ? arm_code::ldr_post : arm_code::str_post);
            else if (!imm && pre && up && !byte && !(bits & 0x70) && op.b != 15 && (!wb || op.a != op.d)) {
                op.code = load ? arm_code::ldr_index : arm_code::str_index;
                op.flags = wb;
            }
        }
        return op;
    }
    inline arm_op decode_arm_page(std::uint32_t bits) {
        const auto ins = decode(bits, false);
        auto op = specialize_arm(ins);
        if (op.code == arm_code::other) {
            if (ins.op == opcode::data)
                op.code = arm_code::data;
            op.generic = ins.op;
            op.value = bits;
        }
        return op;
    }

    inline void fuse_arm(arm_op& first, const arm_op& second) {
        if (first.cond != 14)
            return;
        if (first.code == arm_code::cmp_r && second.code == arm_code::branch && second.cond < 14) {
            first.code = arm_code::cmp_branch;
            first.value = second.value;
            first.flags = second.cond;
        } else if (first.code == arm_code::str_index && first.flags && second.code == arm_code::str && second.cond == 14
            && second.value == 4 && first.a == second.a && second.d != first.a) {
            first.code = arm_code::str_pair;
            first.value = second.d;
        }
    }

}
