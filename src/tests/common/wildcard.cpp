/*
 * Copyright (c) 2026 EKA2L1 Team.
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>

#include <common/wildcard.h>

#include <string>

using namespace eka2l1;

TEST_CASE("Wildcard matches regex metacharacters literally", "[wildcard]") {
    REQUIRE(common::full_wildcard_match<char>("Memo(01).amr", "Memo(01).amr", true));
    REQUIRE_FALSE(common::full_wildcard_match<char>("Memo01.amr", "Memo(01).amr", true));
    REQUIRE(common::full_wildcard_match<char>("a+b^c$d|e{1}[x].txt", "a+b^c$d|e{1}[x].txt", false));
    REQUIRE_FALSE(common::full_wildcard_match<char>("aab", "a+b", false));
    REQUIRE(common::full_wildcard_match<wchar_t>(L"c:\\Nokia\\Memo(02).AMR", L"C:\\NOKIA\\memo(??).amr", true));
}

TEST_CASE("Wildcard star and question mark", "[wildcard]") {
    REQUIRE(common::full_wildcard_match<char>("voicerecorder.rsc", "*.r??", true));
    REQUIRE_FALSE(common::full_wildcard_match<char>("voicerecorder.rsc", "*.R??", false));
}
