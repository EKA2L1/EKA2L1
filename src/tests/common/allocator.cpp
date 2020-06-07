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

TEST_CASE("bitmap_count_bit_aligned", "bitmap_allocator") {
    common::bitmap_allocator alloc(32 * 3);
    
    // Bitmap 1: All bit on
    alloc.set_word(0, 0xFFFFFFFF);

    // Bitmap 2: 12 bits on
    alloc.set_word(1, 0xF00F00F0);

    // Bitmap 3: 8 bits on
    alloc.set_word(2, 0x00F00F00);

    // First bitmap has 52 bits on, plus with bitmap 2 and 3, we got 52
    REQUIRE(alloc.allocated_count(0, 32 * 3 - 1) == 52);
}

TEST_CASE("bitmap_count_unaligned_start", "bitmap_allocator") {
    common::bitmap_allocator alloc(32 * 3);
    
    // Bitmap 1: Valid bit count starting from bit 6
    alloc.set_word(0, 0b110011000011);

    // Bitmap 2: 12 bits on
    alloc.set_word(1, 0xF00F00F0);

    // Bitmap 3: 8 bits on
    alloc.set_word(2, 0x00F00F00);

    // First bitmap has 4 valid bits on (from offset 2), plus with bitmap 2 and 3, we got 24
    REQUIRE(alloc.allocated_count(2, 32 * 3 - 3) == 24);
}

TEST_CASE("bitmap_count_unaligned_start_end", "bitmap_allocator") {
    common::bitmap_allocator alloc(32 * 3);
    
    // Bitmap 1: Valid bit count starting from bit 6
    alloc.set_word(0, 0b110011000011);

    // Bitmap 2: 12 bits on
    alloc.set_word(1, 0xF00F00F0);

    // Bitmap 3: 4 bits valid before offset 68
    alloc.set_word(2, 0b10101110001111);

    // First bitmap has 4 valid bits on (from offset 2), plus with bitmap 2 and 3 (4 bits before offset 70),
    // we got 4 + 12 + 4 = 20 bits 
    REQUIRE(alloc.allocated_count(2, 70) == 20);
}