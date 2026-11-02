/*
 * Copyright (c) 2026 EKA2L1 Team.
 * This file is part of EKA2L1, licensed under GPL version 3 or later.
 */

#include <catch2/catch.hpp>
#include <common/fileutils.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {
    struct copy_fixture {
        const std::string root = "fileutils_copy_test";
        const std::string source = root + "/source.jar";
        const std::string contents = "JAR contents must survive copying onto the same file";

        copy_fixture() {
            eka2l1::common::delete_folder(root);
            eka2l1::common::create_directories(root);
            std::ofstream(source, std::ios::binary) << contents;
        }

        ~copy_fixture() {
            eka2l1::common::delete_folder(root);
        }

        std::string read(const std::string &path) const {
            std::ifstream file(path, std::ios::binary);
            return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        }
    };
}

TEST_CASE("copy_file preserves the source when the destination is the same file", "[fileutils]") {
    copy_fixture fixture;
    std::string destination;

    SECTION("identical path") {
        destination = fixture.source;
    }

    SECTION("path alias") {
        destination = fixture.root + "/./source.jar";
    }

    SECTION("hard link") {
        destination = fixture.root + "/linked.jar";
        std::error_code error;
        std::filesystem::create_hard_link(fixture.source, destination, error);
        REQUIRE_FALSE(error);
    }

    REQUIRE_FALSE(eka2l1::common::copy_file(fixture.source, destination, true));
    REQUIRE(fixture.read(fixture.source) == fixture.contents);
    REQUIRE(fixture.read(destination) == fixture.contents);
}

TEST_CASE("copy_file still copies to a different file", "[fileutils]") {
    copy_fixture fixture;
    const std::string destination = fixture.root + "/copy.jar";

    SECTION("new destination") {
    }

    SECTION("existing destination") {
        std::ofstream(destination, std::ios::binary) << "Old contents";
    }

    REQUIRE(eka2l1::common::copy_file(fixture.source, destination, true));
    REQUIRE(fixture.read(fixture.source) == fixture.contents);
    REQUIRE(fixture.read(destination) == fixture.contents);
}
