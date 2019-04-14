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
#include <common/pystr.h>

using namespace eka2l1;

TEST_CASE("split_whitespace_norm", "pystr") {
    common::pystr test("Hi I'm a string!");
    const auto seps = test.split();

    REQUIRE(seps.size() == 4);
    REQUIRE(seps[0] == "Hi");
    REQUIRE(seps[1] == "I'm");
    REQUIRE(seps[2] == "a");
    REQUIRE(seps[3] == "string!");
}

TEST_CASE("split_whitespace_dup", "pystr") {
    common::pystr test("Hi I'm tri  pii   ng!");
    const auto seps = test.split();

    REQUIRE(seps.size() == 5);
    REQUIRE(seps[0] == "Hi");
    REQUIRE(seps[1] == "I'm");
    REQUIRE(seps[2] == "tri");
    REQUIRE(seps[3] == "pii");
    REQUIRE(seps[4] == "ng!");
}

TEST_CASE("split_custom_str", "pystr") {
    common::pystr test("Hi, I'm, a string!, , which is really cool");
    const auto seps = test.split(", ");

    REQUIRE(seps.size() == 4);
    REQUIRE(seps[0] == "Hi");
    REQUIRE(seps[1] == "I'm");
    REQUIRE(seps[2] == "a string!");
    REQUIRE(seps[3] == "which is really cool");
}

TEST_CASE("str_to_int_base_10", "pystr") {
    common::pystr test("10202");
    REQUIRE(test.as_int<int>() == 10202);
}

TEST_CASE("str_to_int_base_16", "pystr") {
    common::pystr test("12DEA9");
    REQUIRE(test.as_int<int>(0, 16) == 0x12DEA9);
}

TEST_CASE("str_to_int_16_base_fail_intended", "pystr") {
    common::pystr test("12DEA9g");
    REQUIRE(test.as_int<int>(12, 16) == 12);
}

TEST_CASE("str_to_int_2_prefix", "pystr") {
    common::pystr test("0b11011");
    REQUIRE(test.as_int<int>() == 0b11011);
}

TEST_CASE("str_to_fp_no_fract", "pystr") {
    common::pystr test("60");
    REQUIRE(test.as_fp<float>() == 60.0f);
}

TEST_CASE("str_to_fp_fract_neg", "pystr") {
    common::pystr test("-60.56");
    REQUIRE(test.as_fp<float>() == -60.56f);
}
