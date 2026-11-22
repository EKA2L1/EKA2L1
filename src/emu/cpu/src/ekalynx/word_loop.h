// Copyright (c) 2026 EKA2L1 Team. Licensed under GPLv2 or later.
#pragma once

#include "decode.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <optional>

namespace eka2l1::arm::ekalynx {
    struct word_loop {
        static constexpr unsigned capacity = 16;
        std::array<arm_op, capacity> body;
        unsigned length, counter, limit, modified;
        bool reverse_compare;
    };

    inline std::optional<word_loop> analyze_word_loop(const arm_op* ops, unsigned length) {
        using enum arm_code;
        if (length < 4 || length > word_loop::capacity)
            return {};
        const auto& compare = ops[length - 2];
        const auto& back = ops[length - 1];
        if (back.code != branch || back.value != 1u - length || compare.code != cmp_r || compare.cond != 14)
            return {};
        if (back.cond != 11 && back.cond != 12)
            return {};
        const bool reverse = back.cond == 12;
        const auto counter = reverse ? compare.b : compare.a;
        const auto limit = reverse ? compare.a : compare.b;
        unsigned changed = 0, inputs = 0, increments = 0, stores = 0;
        for (unsigned i = 0; i + 2 < length; ++i) {
            const auto& op = ops[i];
            if (op.cond != 14)
                return {};
            if ((op.code == ldm || op.code == stm) && (op.b || (op.value & 0x8000)))
                return {};
            const unsigned a = 1u << op.a, d = 1u << op.d;
            unsigned read = 0, write = 0;
            switch (op.code) {
            case add_i:
            case sub_i:
            case ldr:
                read = a;
                write = d;
                break;
            case add_lsl:
                read = a | (1u << op.b);
                write = d;
                break;
            case mov_i:
                write = d;
                break;
            case mov_lsl:
                read = 1u << op.b;
                write = d;
                break;
            case ldm:
                read = a;
                write = op.value;
                break;
            case stm:
                read = a | op.value;
                stores += std::popcount(op.value);
                break;
            case str:
                read = a | d;
                ++stores;
                break;
            case str_index:
                read = a | (1u << op.b) | d;
                write = op.flags ? a : 0;
                ++stores;
                break;
            default:
                return {};
            }
            if (write & (1u << counter)) {
                if (op.code != add_i || op.a != counter || op.value != 1)
                    return {};
                ++increments;
            }
            inputs |= read & ~changed;
            changed |= write;
        }
        if (!stores || stores > word_loop::capacity || increments != 1 || ((inputs & changed) & ~(1u << counter))
            || limit == counter)
            return {};
        word_loop loop {};
        std::copy_n(ops, length, loop.body.begin());
        loop.length = length;
        loop.counter = counter;
        loop.limit = limit;
        loop.modified = changed;
        loop.reverse_compare = reverse;
        return loop;
    }

    struct linear_word {
        std::uint32_t value, stride;
        linear_word plus(linear_word other) const { return { value + other.value, stride + other.stride }; }
        linear_word offset(std::uint32_t n) const { return { value + n, stride }; }
        linear_word shifted(unsigned n) const { return { value << n, stride << n }; }
    };

    template <typename Lookup>
    unsigned run_word_loop(const word_loop& loop, std::array<std::uint32_t, 16>& regs, unsigned budget, Lookup lookup) {
        using enum arm_code;
        unsigned count = budget / loop.length;
        if (count < 4 || static_cast<std::int32_t>(regs[loop.counter]) < 0)
            return 0;
        std::array<linear_word, 16> values;
        for (unsigned r = 0; r < values.size(); ++r)
            values[r] = { regs[r], unsigned(r == loop.counter) };
        struct store_word {
            std::uint8_t* host;
            unsigned stride, value;
        };
        std::array<store_word, word_loop::capacity> stores;
        std::array<std::uintptr_t, word_loop::capacity> reads;
        unsigned stored = 0, loaded = 0;
        auto read = [&](linear_word address, unsigned reg) {
            if (address.stride || loaded == reads.size())
                return false;
            auto* src = lookup(address.value, 4, false);
            if (!src)
                return false;
            reads[loaded++] = reinterpret_cast<std::uintptr_t>(src);
            values[reg] = {};
            std::memcpy(&values[reg].value, src, 4);
            return true;
        };
        auto write = [&](linear_word address, unsigned reg) {
            if (!address.stride || values[reg].stride)
                return false;
            auto* dst = lookup(address.value, 4, true);
            if (!dst)
                return false;
            count = std::min(count, 1 + (4092 - (address.value & 4095)) / address.stride);
            stores[stored++] = { dst, address.stride, values[reg].value };
            return count >= 4;
        };
        for (unsigned i = 0; i + 2 < loop.length; ++i) {
            const auto& op = loop.body[i];
            auto a = values[op.a], b = values[op.b];
            switch (op.code) {
            case add_i:
                values[op.d] = a.offset(op.value);
                break;
            case sub_i:
                values[op.d] = a.offset(-op.value);
                break;
            case mov_i:
                values[op.d] = { op.value, 0 };
                break;
            case mov_lsl:
                values[op.d] = b.shifted(op.shift);
                break;
            case add_lsl:
                values[op.d] = a.plus(b.shifted(op.shift));
                break;
            case ldr:
                if (!read(a.offset(op.value), op.d))
                    return 0;
                break;
            case str:
                if (!write(a.offset(op.value), op.d))
                    return 0;
                break;
            case str_index: {
                const auto address = a.plus(b.shifted(op.shift));
                if (!write(address, op.d))
                    return 0;
                if (op.flags)
                    values[op.a] = address;
                break;
            }
            case stm:
            case ldm: {
                auto address = a.offset(static_cast<std::int8_t>(op.flags));
                for (auto mask = op.value; mask; mask &= mask - 1, address = address.offset(4)) {
                    const auto r = std::countr_zero(mask);
                    if (op.code == stm ? !write(address, r) : !read(address, r))
                        return 0;
                }
                break;
            }
            default:
                return 0;
            }
        }
        const auto limit = values[loop.limit];
        if (limit.stride || static_cast<std::int32_t>(limit.value) <= static_cast<std::int32_t>(regs[loop.counter]))
            return 0;
        count = std::min(count, limit.value - regs[loop.counter]);
        if (count < 4)
            return 0;

        // Invariant loads must retain their value across the entire batch, including virtual aliases.
        for (unsigned i = 0; i < stored; ++i) {
            const auto begin = reinterpret_cast<std::uintptr_t>(stores[i].host);
            const auto end = begin + (count - 1) * stores[i].stride + 4;
            for (unsigned j = 0; j < loaded; ++j)
                if (begin < reads[j] + 4 && reads[j] < end)
                    return 0;
        }
        for (unsigned i = 0; i < count; ++i)
            for (unsigned j = 0; j < stored; ++j)
                std::memcpy(stores[j].host + i * stores[j].stride, &stores[j].value, 4);
        for (auto mask = loop.modified; mask; mask &= mask - 1) {
            const auto r = std::countr_zero(mask);
            regs[r] = values[r].value + (count - 1) * values[r].stride;
        }
        return count;
    }
}
