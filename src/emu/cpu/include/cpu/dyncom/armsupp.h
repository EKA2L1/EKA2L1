// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <common/types.h>

#define BITS(s, a, b) ((s << ((sizeof(s) * 8 - 1) - b)) >> (sizeof(s) * 8 - b + a - 1))
#define BIT(s, n) ((s >> (n)) & 1)

#define POS(i) ((~(i)) >> 31)
#define NEG(i) ((i) >> 31)

bool AddOverflow(std::uint32_t, std::uint32_t, std::uint32_t);
bool SubOverflow(std::uint32_t, std::uint32_t, std::uint32_t);

// Hot path: called by every ADD/ADC/SUB/SBC/RSB/RSC/CMP/CMN. Inlined so the
// add/sub instruction handlers pay no call overhead; the 64-bit sum carries
// the carry-out in bit 32 and folds to a single host add on 64-bit targets. Overflow keeps the canonical
// "32-bit signed result differs from the exact sum" definition (provably
// correct) and is only computed when the caller wants the flag.
inline std::uint32_t AddWithCarry(std::uint32_t left, std::uint32_t right, std::uint32_t carry_in,
    bool *carry_out_occurred, bool *overflow_occurred) {
    const std::uint64_t unsigned_sum = static_cast<std::uint64_t>(left)
        + static_cast<std::uint64_t>(right) + static_cast<std::uint64_t>(carry_in);
    const std::uint32_t result = static_cast<std::uint32_t>(unsigned_sum);

    if (carry_out_occurred)
        *carry_out_occurred = (unsigned_sum >> 32) != 0;

    if (overflow_occurred) {
        const std::int64_t signed_sum = static_cast<std::int64_t>(static_cast<std::int32_t>(left))
            + static_cast<std::int32_t>(right) + static_cast<std::int64_t>(carry_in);
        *overflow_occurred = (static_cast<std::int64_t>(static_cast<std::int32_t>(result)) != signed_sum);
    }

    return result;
}

bool ARMul_AddOverflowQ(std::uint32_t, std::uint32_t);

std::uint8_t ARMul_SignedSaturatedAdd8(std::uint8_t, std::uint8_t);
std::uint8_t ARMul_SignedSaturatedSub8(std::uint8_t, std::uint8_t);
std::uint16_t ARMul_SignedSaturatedAdd16(std::uint16_t, std::uint16_t);
std::uint16_t ARMul_SignedSaturatedSub16(std::uint16_t, std::uint16_t);

std::uint8_t ARMul_UnsignedSaturatedAdd8(std::uint8_t, std::uint8_t);
std::uint16_t ARMul_UnsignedSaturatedAdd16(std::uint16_t, std::uint16_t);
std::uint8_t ARMul_UnsignedSaturatedSub8(std::uint8_t, std::uint8_t);
std::uint16_t ARMul_UnsignedSaturatedSub16(std::uint16_t, std::uint16_t);
std::uint8_t ARMul_UnsignedAbsoluteDifference(std::uint8_t, std::uint8_t);
std::uint32_t ARMul_SignedSatQ(std::int32_t, std::uint8_t, bool *);
std::uint32_t ARMul_UnsignedSatQ(std::int32_t, std::uint8_t, bool *);
