/*
 * Copyright (c) 2019 EKA2L1 Team
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
#include <common/crypt.h>

#include <cstring>

using namespace eka2l1;

TEST_CASE("base64_encode_unaligned_1", "base64") {
    std::string results;
    results.resize(4);

    const char *source = "A";

    const std::size_t written = crypt::base64_encode(reinterpret_cast<const std::uint8_t *>(source), 1,
        &results[0], 4);

    REQUIRE(written == 4);
    REQUIRE(results == "QQ==");
}

TEST_CASE("base64_encode_unaligned_2", "base64") {
    std::string results;
    results.resize(12);

    const char *source = "AcE00012";

    const std::size_t written = crypt::base64_encode(reinterpret_cast<const std::uint8_t *>(source), 8,
        &results[0], 12);

    REQUIRE(written == 12);
    REQUIRE(results == "QWNFMDAwMTI=");
}

TEST_CASE("base64_encode_good", "base64") {
    std::string results;
    results.resize(20);

    const char *source = "degad1asb920123";

    const std::size_t written = crypt::base64_encode(reinterpret_cast<const std::uint8_t *>(source), 15,
        &results[0], 20);

    REQUIRE(written == 20);
    REQUIRE(results == "ZGVnYWQxYXNiOTIwMTIz");
}

TEST_CASE("base64_decode_1", "base64") {
    std::string results;
    results.resize(3);

    const char *source = "QQ==";

    const std::size_t written = crypt::base64_decode(reinterpret_cast<const std::uint8_t *>(source), 4,
        &results[0], 3);

    REQUIRE(written == 1);
    REQUIRE(strncmp(&results[0], "A", 1) == 0);
}

TEST_CASE("base64_decode_2", "base64") {
    std::string results;
    results.resize(10);

    const char *source = "QWNFMDAwMTI=";

    const std::size_t written = crypt::base64_decode(reinterpret_cast<const std::uint8_t *>(source), 12,
        &results[0], 10);

    REQUIRE(written == 8);
    REQUIRE(strncmp("AcE00012", &results[0], 8) == 0);
}

TEST_CASE("base64_decode_3", "base64") {
    std::string results;
    results.resize(37);

    const char *source = "ZGVnYWQxYXNiOTIwMTIz";

    const std::size_t written = crypt::base64_decode(reinterpret_cast<const std::uint8_t *>(source), 20,
        &results[0], 37);

    REQUIRE(written == 15);
    REQUIRE(strncmp(&results[0], "degad1asb920123", 15) == 0);
}
