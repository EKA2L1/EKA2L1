// Copyright 2012 Michael Kang, 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>

enum class ARMDecodeStatus { SUCCESS, FAILURE };

ARMDecodeStatus decode_arm_instruction(std::uint32_t instr, int* idx);
