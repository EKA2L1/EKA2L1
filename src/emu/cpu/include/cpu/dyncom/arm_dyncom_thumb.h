/* Copyright (C)
 * 2011 - Michael.Kang blackfin.kang@gmail.com
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

/**
 * @file arm_dyncom_thumb.h
 * @brief The thumb dyncom
 * @author Michael.Kang blackfin.kang@gmail.com
 * @version 78.77
 * @date 2011-11-07
 */

#pragma once

#include <common/types.h>

enum class ThumbDecodeStatus {
    UNDEFINED, // Undefined Thumb instruction
    DECODED,   // Instruction decoded to ARM equivalent
    BRANCH,    // Thumb branch (already processed)
    UNINITIALIZED,
};

// Translates a Thumb mode instruction into its ARM equivalent.
ThumbDecodeStatus TranslateThumbInstruction(std::uint32_t addr, std::uint32_t instr, std::uint32_t *ainstr, std::uint32_t *inst_size);

inline std::uint32_t GetThumbInstruction(std::uint32_t instr, std::uint32_t address) {
    // Normally you would need to handle instruction endianness,
    // however, it is fixed to little-endian on the MPCore, so
    // there's no need to check for this beforehand.
    if ((address & 0x3) != 0)
        return instr >> 16;

    return instr & 0xFFFF;
}
