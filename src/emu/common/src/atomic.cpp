/*
 * Copyright (c) 2020 EKA2L1 Team.
 * Copyright 2020 yuzu Emulator Project.
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

#include <common/atomic.h>
#include <cstdint>

#if _MSC_VER
#include <intrin.h>
#endif

namespace eka2l1::common {
#if _MSC_VER
    template <>
    bool atomic_compare_and_swap(volatile std::uint8_t* pointer, std::uint8_t value, std::uint8_t expected) {
        const std::uint8_t result =
            _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(pointer), value, expected);
        return result == expected;
    }

    template <>
    bool atomic_compare_and_swap(volatile std::uint16_t* pointer, std::uint16_t value, std::uint16_t expected) {
        const std::uint16_t result =
            _InterlockedCompareExchange16(reinterpret_cast<volatile short*>(pointer), value, expected);
        return result == expected;
    }

    template <>
    bool atomic_compare_and_swap(volatile std::uint32_t* pointer, std::uint32_t value, std::uint32_t expected) {
        const std::uint32_t result =
            _InterlockedCompareExchange(reinterpret_cast<volatile long*>(pointer), value, expected);
        return result == expected;
    }

    template <>
    bool atomic_compare_and_swap(volatile std::uint64_t* pointer, std::uint64_t value, std::uint64_t expected) {
        const std::uint64_t result = _InterlockedCompareExchange64(reinterpret_cast<volatile __int64*>(pointer),
                                                        value, expected);
        return result == expected;
    }
#else
    template <>
    bool atomic_compare_and_swap(volatile std::uint8_t* pointer, std::uint8_t value, std::uint8_t expected) {
        return __sync_bool_compare_and_swap(pointer, expected, value);
    }

    template <>
    bool atomic_compare_and_swap(volatile std::uint16_t* pointer, std::uint16_t value, std::uint16_t expected) {
        return __sync_bool_compare_and_swap(pointer, expected, value);
    }

    template <>
    bool atomic_compare_and_swap(volatile std::uint32_t* pointer, std::uint32_t value, std::uint32_t expected) {
        return __sync_bool_compare_and_swap(pointer, expected, value);
    }

    template <>
    bool atomic_compare_and_swap(volatile std::uint64_t* pointer, std::uint64_t value, std::uint64_t expected) {
        return __sync_bool_compare_and_swap(pointer, expected, value);
    }
#endif
}