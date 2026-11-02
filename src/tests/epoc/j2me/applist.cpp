/*
 * Copyright (c) 2026 EKA2L1 Team.
 * This file is part of EKA2L1, licensed under GPL version 3 or later.
 */

#include <catch2/catch.hpp>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <config/app_settings.h>
#include <config/config.h>
#include <j2me/applist.h>
#include <j2me/kmidrun.h>
#include <system/epoc.h>
#include <vfs/vfs.h>

#include <fstream>
#include <iterator>
#include <memory>

namespace {
    struct app_list_fixture {
        eka2l1::config::state conf;
        std::unique_ptr<eka2l1::j2me::app_list> apps;

        app_list_fixture() {
            conf.storage = "j2me_applist_test/";
            eka2l1::common::delete_folder(conf.storage);
            apps = std::make_unique<eka2l1::j2me::app_list>(conf);
        }

        ~app_list_fixture() {
            apps.reset();
            eka2l1::common::delete_folder(conf.storage);
        }
    };

    void require_same_entry(const eka2l1::j2me::app_entry &actual, const eka2l1::j2me::app_entry &expected) {
        REQUIRE(actual.id_ == expected.id_);
        REQUIRE(actual.name_ == expected.name_);
        REQUIRE(actual.author_ == expected.author_);
        REQUIRE(actual.icon_path_ == expected.icon_path_);
        REQUIRE(actual.version_ == expected.version_);
        REQUIRE(actual.title_ == expected.title_);
        REQUIRE(actual.original_title_ == expected.original_title_);
    }
}

TEST_CASE("J2ME reinstall updates only the matching application and keeps its id", "[j2me]") {
    app_list_fixture fixture;
    eka2l1::j2me::app_entry first{ 0, "Game A", "Vendor A", "a.png", "1.0", "Game A", "Game A" };
    eka2l1::j2me::app_entry second{ 0, "Game B", "Vendor B", "b.png", "1.0", "Game B", "Game B" };
    first.id_ = fixture.apps->add_entry(first, true);
    second.id_ = fixture.apps->add_entry(second, true);
    REQUIRE(first.id_ != static_cast<std::uint32_t>(-1));
    REQUIRE(second.id_ != static_cast<std::uint32_t>(-1));
    REQUIRE(first.id_ != second.id_);

    second.icon_path_ = "updated.png";
    second.title_ = "Updated title";
    second.original_title_ = "Updated original title";
    REQUIRE(fixture.apps->add_entry(second, true) == second.id_);
    fixture.apps->flush();

    const auto first_result = fixture.apps->get_entry(first.id_);
    const auto second_result = fixture.apps->get_entry(second.id_);
    REQUIRE(first_result.has_value());
    REQUIRE(second_result.has_value());
    require_same_entry(*first_result, first);
    require_same_entry(*second_result, second);
    REQUIRE(fixture.apps->get_entries().size() == 2);

    REQUIRE(fixture.apps->add_entry(second, true) == second.id_);
    REQUIRE(fixture.apps->get_entries().size() == 2);
}

TEST_CASE("J2ME can reinstall its installed JAR without changing the archive", "[j2me]") {
    app_list_fixture fixture;
    eka2l1::config::app_settings settings(&fixture.conf);
    eka2l1::system_create_components components;
    components.conf_ = &fixture.conf;
    components.settings_ = &settings;
    components.cache_root_ = fixture.conf.storage;
    eka2l1::system sys(components);
    auto *io = sys.get_io_system();
    auto filesystem = eka2l1::create_physical_filesystem(epocver::epoc6, "");
    REQUIRE(io->add_filesystem(filesystem).has_value());
    const std::string drive_root = fixture.conf.storage + "c/";
    eka2l1::common::create_directories(drive_root);
    REQUIRE(io->mount_physical_path(drive_c, drive_media::physical, io_attrib_internal,
        eka2l1::common::utf8_to_ucs2(drive_root)));

    eka2l1::j2me::app_entry installed;
    REQUIRE(eka2l1::j2me::install_for_kmidrun(&sys, fixture.apps.get(), "j2meassets/sample.jar", installed)
        == eka2l1::j2me::INSTALL_ERROR_JAR_SUCCESS);
    const auto raw_path = io->get_raw_path(u"C:\\system\\midp\\Test Vendor\\untrusted\\Test MIDlet\\1_090\\Test MIDlet.jar");
    REQUIRE(raw_path.has_value());
    const std::string installed_path = eka2l1::common::ucs2_to_utf8(*raw_path);
    const auto read = [](const std::string &path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    const std::string original = read("j2meassets/sample.jar");
    REQUIRE_FALSE(original.empty());
    REQUIRE(read(installed_path) == original);

    eka2l1::j2me::app_entry reinstalled;
    REQUIRE(eka2l1::j2me::install_for_kmidrun(&sys, fixture.apps.get(), installed_path, reinstalled)
        == eka2l1::j2me::INSTALL_ERROR_JAR_SUCCESS);
    REQUIRE(read(installed_path) == original);
    require_same_entry(reinstalled, installed);
    REQUIRE(fixture.apps->get_entries().size() == 1);
}
