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

#include <catch2/catch.hpp>
#include <common/bytes.h>

#include <cstdint>
#include <cstddef>

using namespace eka2l1;

TEST_CASE("16-bit byte swapping", "Bytes") {
    const std::uint16_t num = 0x125A;
    REQUIRE(common::byte_swap(num) == 0x5A12);
}

TEST_CASE("32-bit byte swapping", "Bytes") {
    const std::uint32_t num = 0x125A3B48;
    REQUIRE(common::byte_swap(num) == 0x483B5A12);
}

TEST_CASE("64-bit byte swapping", "Bytes") {
    const std::uint64_t num = 0x1A2B3C4D5E6F7081;
    REQUIRE(common::byte_swap(num) == 0x81706F5E4D3C2B1A);
}
