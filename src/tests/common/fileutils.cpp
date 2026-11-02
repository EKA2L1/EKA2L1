/*
 * Copyright (c) 2026 EKA2L1 Team.
 * This file is part of EKA2L1, licensed under GPL version 3 or later.
 */

#include <catch2/catch.hpp>
#include <common/fileutils.h>

#include <fstream>
#include <iterator>
#include <string>

TEST_CASE("copy_file rejects copying a file onto itself without truncating it", "[fileutils]") {
    const std::string path = "fileutils_copy_test.jar";
    const std::string contents = "Original JAR contents";
    struct cleanup {
        const std::string &path;
        ~cleanup() { eka2l1::common::remove(path); }
    } cleanup{ path };
    std::ofstream(path, std::ios::binary) << contents;

    REQUIRE_FALSE(eka2l1::common::copy_file(path, path, true));
    std::ifstream file(path, std::ios::binary);
    REQUIRE(std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()) == contents);
}
