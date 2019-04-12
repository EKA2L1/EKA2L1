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

#include <common/bytes.h>
#include <cstddef>
#include <cstdint>

namespace eka2l1::common {
    template <>
    std::uint16_t byte_swap(std::uint16_t val) {
        return (val >> 8) | (val << 8);
    }

    template <>
    std::uint32_t byte_swap(std::uint32_t val) {
        //        AA             BB00                   CC0000                  DD000000
        return (val >> 24) | (val >> 8) & 0xFF00 | (val << 8) & 0xFF0000 | (val << 24) & 0xFF000000;
    }

    template <>
    std::uint64_t byte_swap(std::uint64_t val) {
        val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
        val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);

        return (val << 32) | (val >> 32);
    }
}
