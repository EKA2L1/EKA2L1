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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <catch2/catch.hpp>

#include <common/platform.h>
#include <control/dispatcher.h>
#include <control/new_file.h>

#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#if !EKA2L1_PLATFORM(WIN32)
#include <unistd.h>
#endif

using namespace eka2l1::control;

namespace {
    std::string scratch_path(const char *name) {
        return std::string("ekatests-newfile-") + name;
    }

    std::string contents_of(const std::string &path) {
        std::ifstream file(path, std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        return text.str();
    }

    void put(const std::string &path, const std::string &text) {
        std::ofstream(path, std::ios::binary) << text;
    }

    // The rpc_error write_new_file() throws, or none.
    std::optional<rpc_error> error_writing(const std::string &path, const std::string &data) {
        try {
            write_new_file(path, data);
        } catch (rpc_error &error) {
            return error;
        }

        return std::nullopt;
    }
}

TEST_CASE("a_new_file_is_written", "[control][new_file]") {
    const std::string path = scratch_path("fresh.png");
    std::remove(path.c_str());

    REQUIRE_FALSE(error_writing(path, std::string("PNG\0data", 8)).has_value());
    REQUIRE(contents_of(path) == std::string("PNG\0data", 8));

    std::remove(path.c_str());
}

TEST_CASE("an_existing_file_is_left_alone", "[control][new_file]") {
    const std::string path = scratch_path("existing.png");
    put(path, "keep me");

    const std::optional<rpc_error> error = error_writing(path, "new");
    REQUIRE(error.has_value());
    REQUIRE(error->code() == error_failed);
    REQUIRE(std::string(error->what()).find(path) != std::string::npos);
    REQUIRE(contents_of(path) == "keep me");

    std::remove(path.c_str());
}

#if !EKA2L1_PLATFORM(WIN32)
TEST_CASE("a_symbolic_link_is_not_followed", "[control][new_file]") {
    const std::string target = scratch_path("target.txt");
    const std::string dangling = scratch_path("dangling.txt");
    const std::string link = scratch_path("link.png");
    std::remove(link.c_str());
    std::remove(dangling.c_str());

    // To a file: it stays as it is.
    put(target, "keep me");
    REQUIRE(symlink(target.c_str(), link.c_str()) == 0);
    REQUIRE(error_writing(link, "new").has_value());
    REQUIRE(contents_of(target) == "keep me");
    std::remove(link.c_str());

    // To nothing: nothing is made there.
    REQUIRE(symlink(dangling.c_str(), link.c_str()) == 0);
    REQUIRE(error_writing(link, "new").has_value());
    REQUIRE(access(dangling.c_str(), F_OK) != 0);

    std::remove(link.c_str());
    std::remove(target.c_str());
}
#endif
