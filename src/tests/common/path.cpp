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
#include <common/path.h>
#include <cstring>

TEST_CASE("root_name", "path_resolving_test") {
    const std::string example_path_sym = "C:\\symemu\\";
    REQUIRE(eka2l1::root_name(example_path_sym, true) == "C:");
}

TEST_CASE("absolute_checking", "path_resolving_test") {
    const std::string example_path = "Z:\\resource\\";
    const std::string current_dir = "Z:\\sys\\bin\\";

    REQUIRE(eka2l1::is_absolute(example_path, current_dir, true));
}

TEST_CASE("absolute", "path_resolving_test") {
    const std::string example_path = "despacito";
    const std::string current_dir = "Z:\\sys\\bin\\";
    const std::string expected = "Z:\\sys\\bin\\despacito";

    REQUIRE(eka2l1::absolute_path(example_path, current_dir, true) == expected);
}

TEST_CASE("add_path_mess", "path_resolving_test") {
    const std::string example_path = "Z:\\sys/bin\\hi";
    const std::string example_path2 = "ha/ma";
    const std::string expected_result = "Z:\\sys\\bin\\hi\\ha\\ma";

    REQUIRE(eka2l1::add_path(example_path, example_path2, true) == expected_result);
}

TEST_CASE("filename", "path_resolving_test") {
    const std::string example_path = "Z:\\sys\\bin\\euser.dll";
    REQUIRE(strncmp(eka2l1::filename(example_path, true).data(), "euser.dll", 9) == 0);
}

TEST_CASE("filedirectory", "path_resolving_test") {
    const std::string example_path = "Z:\\sys\\bin\\euser.dll";
    REQUIRE(strncmp(eka2l1::file_directory(example_path, true).data(), "Z:\\sys\\bin\\", 11) == 0);
}

TEST_CASE("file_directory_directory_alone", "path_resolving_test") {
    const std::string example_path = "Z:\\sys\\bin\\";
    REQUIRE(strncmp(eka2l1::file_directory(example_path, true).data(), "Z:\\sys\\bin\\", 11) == 0);
}

TEST_CASE("path_iterator", "path_resolving_test") {
    const std::string path = "z:\\private\\10202bef\\";
    const std::string expected_components[3] = { "z:", "private", "10202bef" };

    eka2l1::path_iterator iterator(path);
    std::size_t i = 0;

    for (; iterator; i++, iterator++) {
        if (i >= 3) {
            REQUIRE(*iterator == "");
            break;
        }

        REQUIRE(expected_components[i] == *iterator);
    }

    REQUIRE(i == 3);
}

TEST_CASE("path_iterator_file", "path_resolving_test") {
    const std::string path = "Z:\\sys\\bin\\euser.dll";
    const std::string expected_components[4] = { "Z:", "sys", "bin", "euser.dll" };

    eka2l1::path_iterator iterator(path);
    std::size_t i = 0;

    for (; iterator; i++, iterator++) {
        if (i >= 4) {
            REQUIRE(*iterator == "");
            break;
        }

        REQUIRE(expected_components[i] == *iterator);
    }

    REQUIRE(4 == i);
}

TEST_CASE("extension", "path_resolving_test") {
    std::string test_path = "hiyou.ne.py";
    std::string result_ext = eka2l1::path_extension(test_path);

    REQUIRE(result_ext == ".py");
}

TEST_CASE("replace_extension", "path_resolving_test") {
    std::string test_path = "hiyou.ne.py";
    std::string expected = eka2l1::replace_extension(test_path, ".mom");

    REQUIRE(expected == "hiyou.ne.mom");
}
