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
#include <services/loader/loader.h>

TEST_CASE("Library search paths preserve directory order and accept omitted separators", "[loader]") {
    // RLibrary::Load's semicolon list contains directories; trailing separators are optional.
    auto paths = eka2l1::get_library_search_paths(u"E:\\System\\Apps\\Example;C:\\System\\Libs\\");
    REQUIRE(paths == std::vector<std::u16string>{ u"E:\\System\\Apps\\Example\\", u"C:\\System\\Libs\\" });
    REQUIRE(paths.front() + u"library.dll" == u"E:\\System\\Apps\\Example\\library.dll");
    REQUIRE(eka2l1::get_library_search_paths(u";\\System\\Libs;;") == std::vector<std::u16string>{ u"\\System\\Libs\\" });
    REQUIRE(eka2l1::get_library_search_paths(u";;").empty());
    REQUIRE(eka2l1::get_library_search_paths(u"").empty());
}
