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

#include <cstddef>
#include <cstdint>
#include <type_traits>

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

    template <typename T>
    bool bit(const T num, const std::uint8_t idx) {
        return num & (1 << idx);
    }

    template <typename T>
    constexpr std::uint8_t bit_size() {
        if constexpr(std::is_same_v<T, std::uint8_t>) {
            return 8;
        }

        if constexpr(std::is_same_v<T, std::uint16_t>) {
            return 16;
        }

        if constexpr(std::is_same_v<T, std::uint32_t>) {
            return 32;
        }

        if constexpr(std::is_same_v<T, std::uint64_t>) {
            return 64;
        }

        return 0;
    }

    template <typename T>
    constexpr T rotate_right(T value, std::uint8_t amount) {
        constexpr std::uint8_t bsize = bit_size<T>();
        if (amount % bsize == 0) {
            return value;
        }

        return static_cast<T>((value >> amount) | (value << (bsize - amount)));
    }

    /**
     * Sign extended original value with bit_count to full bit width of T
     *
     * @tparam bit_count        The number of original bits.
     * @tparam T                The type to sign-extended to.
     *
     * @param value             The value to sign-extended, expected to have bit_count bits.
     * @return                  Fully sign-extended value.
     */
    template <size_t bit_count, typename T>
    T sign_extended(const T value) {
        constexpr T mask = static_cast<T>(1 << bit_count) - 1;
        if (value & static_cast<T>(1 << (bit_count - 1))) {
            return value | ~mask;
        }

        return value;
    }

    /**
     * Sign extended original value with bit_count to full bit width of T
     *
     * @tparam T                The type to sign-extended to.
     *
     * @param bit_count         The number of original bits.
     * @param value             The value to sign-extended, expected to have bit_count bits.
     *
     * @return                  Fully sign-extended value.
     */
    template <typename T>
    T sign_extended(size_t bit_count, const T value) {
        const T mask = static_cast<T>(1 << bit_count) - 1;
        if (value & static_cast<T>(1 << (bit_count - 1))) {
            return value | ~mask;
        }

        return value;
    }
}
