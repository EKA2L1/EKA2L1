/*  armsupp.c -- ARMulator support code:  ARM6 Instruction Emulator.
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

#include <cpu/dyncom/armsupp.h>

// Unsigned sum of absolute difference
std::uint8_t ARMul_UnsignedAbsoluteDifference(std::uint8_t left, std::uint8_t right) {
    if (left > right)
        return left - right;

    return right - left;
}

// Add with carry, indicates if a carry-out or signed overflow occurred.
std::uint32_t AddWithCarry(std::uint32_t left, std::uint32_t right, std::uint32_t carry_in, bool* carry_out_occurred,
                 bool* overflow_occurred) {
    std::uint64_t unsigned_sum = (std::uint64_t)left + (std::uint64_t)right + (std::uint64_t)carry_in;
    std::int64_t signed_sum = (std::int64_t)(std::int32_t)left + (std::int64_t)(std::int32_t)right + (std::int64_t)carry_in;
    std::uint64_t result = (unsigned_sum & 0xFFFFFFFF);

    if (carry_out_occurred)
        *carry_out_occurred = (result != unsigned_sum);

    if (overflow_occurred)
        *overflow_occurred = ((std::int64_t)(std::int32_t)result != signed_sum);

    return (std::uint32_t)result;
}

// Compute whether an addition of A and B, giving RESULT, overflowed.
bool AddOverflow(std::uint32_t a, std::uint32_t b, std::uint32_t result) {
    return ((NEG(a) && NEG(b) && POS(result)) || (POS(a) && POS(b) && NEG(result)));
}

// Compute whether a subtraction of A and B, giving RESULT, overflowed.
bool SubOverflow(std::uint32_t a, std::uint32_t b, std::uint32_t result) {
    return ((NEG(a) && POS(b) && POS(result)) || (POS(a) && NEG(b) && NEG(result)));
}

// Returns true if the Q flag should be set as a result of overflow.
bool ARMul_AddOverflowQ(std::uint32_t a, std::uint32_t b) {
    std::uint32_t result = a + b;
    if (((result ^ a) & (std::uint32_t)0x80000000) && ((a ^ b) & (std::uint32_t)0x80000000) == 0)
        return true;

    return false;
}

// 8-bit signed saturated addition
std::uint8_t ARMul_SignedSaturatedAdd8(std::uint8_t left, std::uint8_t right) {
    std::uint8_t result = left + right;

    if (((result ^ left) & 0x80) && ((left ^ right) & 0x80) == 0) {
        if (left & 0x80)
            result = 0x80;
        else
            result = 0x7F;
    }

    return result;
}

// 8-bit signed saturated subtraction
std::uint8_t ARMul_SignedSaturatedSub8(std::uint8_t left, std::uint8_t right) {
    std::uint8_t result = left - right;

    if (((result ^ left) & 0x80) && ((left ^ right) & 0x80) != 0) {
        if (left & 0x80)
            result = 0x80;
        else
            result = 0x7F;
    }

    return result;
}

// 16-bit signed saturated addition
std::uint16_t ARMul_SignedSaturatedAdd16(std::uint16_t left, std::uint16_t right) {
    std::uint16_t result = left + right;

    if (((result ^ left) & 0x8000) && ((left ^ right) & 0x8000) == 0) {
        if (left & 0x8000)
            result = 0x8000;
        else
            result = 0x7FFF;
    }

    return result;
}

// 16-bit signed saturated subtraction
std::uint16_t ARMul_SignedSaturatedSub16(std::uint16_t left, std::uint16_t right) {
    std::uint16_t result = left - right;

    if (((result ^ left) & 0x8000) && ((left ^ right) & 0x8000) != 0) {
        if (left & 0x8000)
            result = 0x8000;
        else
            result = 0x7FFF;
    }

    return result;
}

// 8-bit unsigned saturated addition
std::uint8_t ARMul_UnsignedSaturatedAdd8(std::uint8_t left, std::uint8_t right) {
    std::uint8_t result = left + right;

    if (result < left)
        result = 0xFF;

    return result;
}

// 16-bit unsigned saturated addition
std::uint16_t ARMul_UnsignedSaturatedAdd16(std::uint16_t left, std::uint16_t right) {
    std::uint16_t result = left + right;

    if (result < left)
        result = 0xFFFF;

    return result;
}

// 8-bit unsigned saturated subtraction
std::uint8_t ARMul_UnsignedSaturatedSub8(std::uint8_t left, std::uint8_t right) {
    if (left <= right)
        return 0;

    return left - right;
}

// 16-bit unsigned saturated subtraction
std::uint16_t ARMul_UnsignedSaturatedSub16(std::uint16_t left, std::uint16_t right) {
    if (left <= right)
        return 0;

    return left - right;
}

// Signed saturation.
std::uint32_t ARMul_SignedSatQ(std::int32_t value, std::uint8_t shift, bool* saturation_occurred) {
    const std::uint32_t max = (1 << shift) - 1;
    const std::int32_t top = (value >> shift);

    if (top > 0) {
        *saturation_occurred = true;
        return max;
    } else if (top < -1) {
        *saturation_occurred = true;
        return ~max;
    }

    *saturation_occurred = false;
    return (std::uint32_t)value;
}

// Unsigned saturation
std::uint32_t ARMul_UnsignedSatQ(std::int32_t value, std::uint8_t shift, bool* saturation_occurred) {
    const std::uint32_t max = (1 << shift) - 1;

    if (value < 0) {
        *saturation_occurred = true;
        return 0;
    } else if ((std::uint32_t)value > max) {
        *saturation_occurred = true;
        return max;
    }

    *saturation_occurred = false;
    return (std::uint32_t)value;
}
