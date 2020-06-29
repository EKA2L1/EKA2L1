/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>

namespace eka2l1::common {
    template <typename T>
    T byte_swap(T val);

    enum endian_type {
        little_endian = 0x00000001,
        big_endian = 0x01000000
    };

    /**
     * \brief Get system endian type.
     * 
     * Can be use at both compile-time and run-time. Only supports little and big endian.
     */
    constexpr endian_type get_system_endian_type() {
        return ((0xFFFFFFFF & 0x1) == little_endian) ? little_endian : big_endian;
    }

    template <typename T>
    T extract_bits(const T num, const std::uint8_t p, const std::uint8_t n) { 
        return (((1 << n) - 1) & (num >> (p - 1))); 
    } 
}
