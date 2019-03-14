/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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
#include <common/chunkyseri.h>

#include <vector>

using namespace eka2l1;

TEST_CASE("do_read_generic", "chunkyseri") {
    const char *test_data = "\1\0\0\0\5\5\5\5 \0\0\0PEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE";

    std::vector<std::uint8_t> test_data_vec;
    test_data_vec.resize(45);
    std::copy(test_data, test_data + 45, &test_data_vec[0]);

    common::chunkyseri seri(reinterpret_cast<std::uint8_t *>(&test_data_vec[0]), 45, common::SERI_MODE_READ);
    int simple_num1 = 0;
    std::uint32_t simple_num2 = 0;

    seri.absorb(simple_num1);
    seri.absorb(simple_num2);

    std::string str;
    seri.absorb(str);

    // Little endian
    // TODO (Big endian)
    REQUIRE(simple_num1 == 1);
    REQUIRE(simple_num2 == 0x05050505);
    REQUIRE(str == "PEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE");
}

TEST_CASE("do_measure_generic", "chunkyseri") {
    common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MESAURE);
    std::uint32_t dummy1;
    std::uint32_t dummy2;

    std::uint16_t dummy3;
    std::string dummy4 = "LOOL";
    std::u16string dummy5 = u"LOOL";

    seri.absorb(dummy1);
    seri.absorb(dummy2);
    seri.absorb(dummy3);
    seri.absorb(dummy4);
    seri.absorb(dummy5);

    // Calculate size
    std::size_t expected = sizeof(dummy1) + sizeof(dummy2) + sizeof(dummy3) + 4 + dummy4.length()
        + 4 + dummy5.length() * 2;

    REQUIRE(seri.size() == expected);
}

TEST_CASE("do_write_generic", "chunkyseri") {
    std::vector<std::uint8_t> buf;
    buf.resize(32);

    std::uint64_t d1 = 0xDFFFFFFFFF;
    std::uint32_t d2 = 15;

    std::string d3 = "HELLO!";

    common::chunkyseri seri(&buf[0], 32, common::SERI_MODE_WRITE);
    seri.absorb(d1);
    seri.absorb(d2);
    seri.absorb(d3);

    std::uint8_t *bufptr = &buf[0];
    REQUIRE(d1 == *reinterpret_cast<decltype(d1) *>(bufptr));
    bufptr += sizeof(d1);

    REQUIRE(d2 == *reinterpret_cast<decltype(d2) *>(bufptr));
    bufptr += sizeof(d2);

    REQUIRE(d3.length() == *reinterpret_cast<std::uint32_t *>(bufptr));
    bufptr += 4;

    REQUIRE(d3 == reinterpret_cast<char *>(bufptr));
}

TEST_CASE("do_read_with_section", "chunkyseri") {
    const char *buf = "TestSection\1\0\5\0\7\0\7\0\0\0HIPEOPL";

    std::vector<std::uint8_t> buf_vec;
    buf_vec.resize(29);
    std::copy(buf, buf + 29, &buf_vec[0]);

    common::chunkyseri seri(reinterpret_cast<std::uint8_t *>(&buf_vec[0]), 29, common::SERI_MODE_READ);
    REQUIRE(seri.section("TestSection", 1));

    std::uint16_t t1 = 0;
    std::uint16_t t2 = 0;

    std::string t3{};

    seri.absorb(t1);
    seri.absorb(t2);
    seri.absorb(t3);

    REQUIRE(t1 == 5);
    REQUIRE(t2 == 7);
    REQUIRE(t3 == "HIPEOPL");
}
