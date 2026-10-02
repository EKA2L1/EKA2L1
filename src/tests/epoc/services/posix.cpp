/*
 * Copyright (c) 2026 EKA2L1 Team.
 * This file is part of EKA2L1, licensed under GPL version 3 or later.
 */

#include <catch2/catch.hpp>
#include <common/fileutils.h>
#include <services/posix/posix.h>

#include <cerrno>
#include <fstream>

TEST_CASE("POSIX stat reports files and directories without opening them", "[posix]") {
    struct test_directory {
        test_directory() {
            eka2l1::common::delete_folder("posix_stat_test");
            eka2l1::common::create_directories("posix_stat_test/levels");
            std::ofstream("posix_stat_test/levels/test.wad", std::ios::binary) << "PWADdata";
        }
        ~test_directory() {
            eka2l1::common::delete_folder("posix_stat_test");
        }
    } directory;

    eka2l1::io_system io;
    auto fs = eka2l1::create_physical_filesystem(epocver::epoc70, "");
    io.add_filesystem(fs);
    REQUIRE(io.mount_physical_path(drive_e, drive_media::physical, io_attrib_internal, u"posix_stat_test"));
    eka2l1::posix_file_manager files(&io);
    eka2l1::posix_stat result{};
    int error = 0;

    files.stat(u"E:\\levels\\test.wad", &result, error);
    REQUIRE(error == 0);
    REQUIRE(result.size == 8);
    REQUIRE(result.mode == (0100000 | 0200));
    REQUIRE(result.device == 4);
    REQUIRE(result.special_device == 4);
    REQUIRE(result.link_count == 1);
    REQUIRE(result.block_size == 512);

    const auto handle = files.open(u"E:\\levels\\test.wad", READ_MODE | BIN_MODE, true, false, error);
    REQUIRE(error == 0);
    REQUIRE(handle != 0);
    eka2l1::posix_stat descriptor_result{};
    files.stat(handle, &descriptor_result, error);
    REQUIRE(error == 0);
    REQUIRE(descriptor_result.mode == result.mode);
    REQUIRE(descriptor_result.size == result.size);
    REQUIRE(descriptor_result.modification_time == result.modification_time);
    files.close(handle, error);

    files.stat(u"E:\\levels", &result, error);
    REQUIRE(error == 0);
    REQUIRE((result.mode & 0170000) == 0040000);

    files.stat(u"E:\\missing.wad", &result, error);
    REQUIRE(error == ENOENT);
}
