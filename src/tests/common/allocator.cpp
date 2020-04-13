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
#include <common/allocator.h>

#include <cstddef>
#include <cstdint>

using namespace eka2l1;

TEST_CASE("bitmap_alloc_no_best_fit_only_one_fit", "bitmap_allocator") {
    common::bitmap_allocator alloc(32);

    // Bitmap:              1000 0101 0001 0001 0101 0001 00[11 1]001
    alloc.set_word(0, 0b10000101000100010101000100111001);

    int to_alloc = 3;
    REQUIRE(alloc.allocate_from(0, to_alloc) == 26);
    REQUIRE(to_alloc == 3);

    // After allocate:      1000 0101 0001 0001 0101 0001 00[00 0]001
    REQUIRE(alloc.get_word(0) == 0b10000101000100010101000100000001);
}

TEST_CASE("bitmap_alloc_no_best_fit_multiple_fit", "bitmap_allocator") {
    common::bitmap_allocator alloc(32);

    // Bitmap 1:            1000 0[111] 1001 0001 0101 0001 0011 1001
    alloc.set_word(0, 0b10000111100100010101000100111001);

    int to_alloc = 3;
    REQUIRE(alloc.allocate_from(0, to_alloc) == 5);
    REQUIRE(to_alloc == 3);

    // After allocate:      1000 0[000] 1001 0001 0101 0001 0011 1001
    REQUIRE(alloc.get_word(0) == 0b10000000100100010101000100111001);
}

TEST_CASE("bitmap_alloc_no_best_fit_alloc_across", "bitmap_allocator") {
    common::bitmap_allocator alloc(64);

    alloc.set_word(0, 0b111);
    alloc.set_word(1, 0b11100000000000000000000000000000);

    int to_alloc = 5;
    REQUIRE(alloc.allocate_from(0, to_alloc) == 29);
    REQUIRE(to_alloc == 5);

    REQUIRE(alloc.get_word(0) == 0);
    REQUIRE(alloc.get_word(1) == 0b00100000000000000000000000000000);
}

TEST_CASE("bitmap_alloc_best_fit_multiple_fit", "bitmap_allocator") {
    common::bitmap_allocator alloc(32);

    // Bitmap 1:            1000 0111 1001 0001 0101 0001 00[11 1]001
    alloc.set_word(0, 0b10000111100100010101000100111001);

    int to_alloc = 3;
    REQUIRE(alloc.allocate_from(0, to_alloc, true) == 26);
    REQUIRE(to_alloc == 3);

    // After allocate:      1000 0111 1001 0001 0101 0001 00[00 0]001
    REQUIRE(alloc.get_word(0) == 0b10000111100100010101000100000001);
}
