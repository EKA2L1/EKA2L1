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

std::uint32_t AddWithCarry(std::uint32_t, std::uint32_t, std::uint32_t, bool*, bool*);
bool ARMul_AddOverflowQ(std::uint32_t, std::uint32_t);

std::uint8_t  ARMul_SignedSaturatedAdd8(std::uint8_t, std::uint8_t);
std::uint8_t  ARMul_SignedSaturatedSub8(std::uint8_t, std::uint8_t);
std::uint16_t  ARMul_SignedSaturatedAdd16(std::uint16_t, std::uint16_t);
std::uint16_t  ARMul_SignedSaturatedSub16(std::uint16_t, std::uint16_t);

std::uint8_t  ARMul_UnsignedSaturatedAdd8(std::uint8_t, std::uint8_t);
std::uint16_t  ARMul_UnsignedSaturatedAdd16(std::uint16_t, std::uint16_t);
std::uint8_t  ARMul_UnsignedSaturatedSub8(std::uint8_t, std::uint8_t);
std::uint16_t  ARMul_UnsignedSaturatedSub16(std::uint16_t, std::uint16_t);
std::uint8_t  ARMul_UnsignedAbsoluteDifference(std::uint8_t, std::uint8_t);
std::uint32_t ARMul_SignedSatQ(std::int32_t, std::uint8_t, bool*);
std::uint32_t ARMul_UnsignedSatQ(std::int32_t, std::uint8_t, bool*);
