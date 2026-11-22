// Copyright (c) 2026 EKA2L1 Team. Licensed under GPLv2 or later.
#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>

namespace eka2l1::arm::ekalynx {
    inline std::int32_t saturate_signed(std::int64_t value, std::uint32_t& cpsr) {
        const auto result = std::clamp<std::int64_t>(value, std::numeric_limits<std::int32_t>::min(),
            std::numeric_limits<std::int32_t>::max());
        if (result != value)
            cpsr |= 1u << 27;
        return static_cast<std::int32_t>(result);
    }

    inline std::uint32_t saturate_operand(std::uint32_t bits, std::uint32_t source, std::uint32_t& cpsr) {
        const bool pair = bits & 32, is_unsigned = bits & (1u << 22);
        const unsigned exponent = (bits >> 16) & (pair ? 15 : 31);
        const auto limit = std::int64_t{1} << exponent;
        const auto clamp = [&](std::int32_t value) {
            const auto result = std::clamp<std::int64_t>(value, is_unsigned ? 0 : -limit, limit - 1);
            if (result != value)
                cpsr |= 1u << 27;
            return static_cast<std::uint32_t>(result);
        };
        if (pair) {
            const auto low = clamp(static_cast<std::int16_t>(source));
            const auto high = clamp(static_cast<std::int16_t>(source >> 16));
            return (low & 0xffff) | (high << 16);
        }
        const unsigned amount = (bits >> 7) & 31;
        return clamp(bits & 64 ? static_cast<std::int32_t>(source) >> (amount ? amount : 31)
                               : static_cast<std::int32_t>(source << amount));
    }

    inline std::uint32_t parallel_arithmetic(std::uint32_t bits, std::uint32_t lhs, std::uint32_t rhs,
        std::uint32_t& cpsr) {
        const unsigned shape = (bits >> 5) & 7, mode = (bits >> 20) & 3;
        const bool is_unsigned = bits & (1u << 22);
        const unsigned width = shape >= 4 ? 8 : 16;
        const int limit = 1 << width, sign = limit / 2;
        const bool exchange = shape == 1 || shape == 2;
        if (exchange)
            rhs = std::rotl(rhs, 16);
        const auto lane = [&](std::uint32_t value, unsigned offset) {
            const int raw = (value >> offset) & (limit - 1);
            return !is_unsigned && (raw & sign) ? raw - limit : raw;
        };
        std::uint32_t result = 0, ge = 0;
        for (unsigned offset = 0; offset < 32; offset += width) {
            const bool subtract = exchange ? (shape == 1) == (offset == 0) : (shape & 3) == 3;
            const auto left = lane(lhs, offset), right = lane(rhs, offset);
            auto value = subtract ? left - right : left + right;
            if (mode == 1) {
                if (is_unsigned && !subtract ? value >= limit : value >= 0)
                    ge |= (width == 8 ? 1u : 3u) << (offset / 8);
            } else if (mode == 2) {
                value = std::clamp(value, is_unsigned ? 0 : -sign, is_unsigned ? limit - 1 : sign - 1);
            } else {
                value >>= 1;
            }
            result |= (static_cast<std::uint32_t>(value) & (limit - 1)) << offset;
        }
        if (mode == 1)
            cpsr = (cpsr & ~0x000f0000u) | (ge << 16);
        return result;
    }

    inline void dual_multiply(std::uint32_t bits, std::array<std::uint32_t, 16>& regs, std::uint32_t& flags) {
        const auto rm = bits & 15, rn = (bits >> 16) & 15, rd = (bits >> 12) & 15;
        const auto a = regs[rm];
        auto b = regs[(bits >> 8) & 15];
        if (bits & 32)
            b = std::rotl(b, 16);
        const auto lo = std::int16_t(a) * std::int16_t(b);
        const auto hi = std::int16_t(a >> 16) * std::int16_t(b >> 16);
        const auto product = std::int64_t(lo) + (bits & 64 ? -hi : hi);
        if (bits & (1u << 22)) {
            const auto acc = (std::uint64_t(regs[rn]) << 32) | regs[rd];
            const auto sum = acc + product;
            regs[rn] = static_cast<std::uint32_t>(sum >> 32);
            regs[rd] = static_cast<std::uint32_t>(sum);
        } else {
            const auto sum = product + (rd == 15 ? 0 : std::int32_t(regs[rd]));
            regs[rn] = static_cast<std::uint32_t>(sum);
            if (sum != std::int32_t(sum))
                flags |= 1u << 27;
        }
    }

    inline void signed_multiply(std::uint32_t bits, std::array<std::uint32_t, 16>& regs, std::uint32_t& cpsr) {
        const unsigned dst = (bits >> 16) & 15, acc = (bits >> 12) & 15;
        const auto lhs = regs[bits & 15], rhs = regs[(bits >> 8) & 15];
        const auto half = [](std::uint32_t word, bool high) {
            return static_cast<std::int16_t>(high ? word >> 16 : word);
        };
        const auto kind = (bits >> 21) & 3;
        const auto multiplier = half(rhs, bits & 64);
        const std::int32_t product = kind == 1
            ? static_cast<std::int32_t>((std::int64_t(static_cast<std::int32_t>(lhs)) * multiplier) >> 16)
            : half(lhs, bits & 32) * multiplier;
        if (kind == 2) {
            auto sum = (std::uint64_t(regs[dst]) << 32) | regs[acc];
            sum += product;
            regs[acc] = static_cast<std::uint32_t>(sum);
            regs[dst] = static_cast<std::uint32_t>(sum >> 32);
        } else if (kind == 3 || (kind == 1 && (bits & 32))) {
            regs[dst] = product;
        } else {
            const auto sum = std::int64_t(static_cast<std::int32_t>(regs[acc])) + product;
            regs[dst] = static_cast<std::uint32_t>(sum);
            if (sum != static_cast<std::int32_t>(regs[dst]))
                cpsr |= 1u << 27;
        }
    }
}
