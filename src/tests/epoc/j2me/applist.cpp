/*
 * Copyright (c) 2026 EKA2L1 Team.
 * This file is part of EKA2L1, licensed under GPL version 3 or later.
 */

#include <catch2/catch.hpp>
#include <common/fileutils.h>
#include <config/config.h>
#include <j2me/applist.h>

namespace {
    struct app_list_fixture {
        eka2l1::config::state conf;

        app_list_fixture() {
            conf.storage = "j2me_applist_test/";
            eka2l1::common::delete_folder(conf.storage);
        }

        ~app_list_fixture() {
            eka2l1::common::delete_folder(conf.storage);
        }
    };
}

TEST_CASE("J2ME reinstall updates the intended row without changing its id", "[j2me]") {
    app_list_fixture fixture;
    eka2l1::j2me::app_list apps(fixture.conf);
    eka2l1::j2me::app_entry first{ 0, "Game A", "Vendor", "a.png", "1.0", "Game A", "Game A" };
    eka2l1::j2me::app_entry second{ 0, "Game B", "Vendor", "b.png", "1.0", "Game B", "Game B" };
    first.id_ = apps.add_entry(first, true);
    second.id_ = apps.add_entry(second, true);
    REQUIRE(first.id_ != second.id_);

    second.icon_path_ = "updated.png";
    second.title_ = "Updated title";
    REQUIRE(apps.add_entry(second, true) == second.id_);
    const auto a = apps.get_entry(first.id_);
    const auto b = apps.get_entry(second.id_);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    REQUIRE(a->name_ == first.name_);
    REQUIRE(a->title_ == first.title_);
    REQUIRE(a->version_ == first.version_);
    REQUIRE(b->icon_path_ == second.icon_path_);
    REQUIRE(b->title_ == second.title_);
    REQUIRE(b->original_title_ == second.original_title_);
    REQUIRE(b->version_ == second.version_);
    REQUIRE(apps.get_entries().size() == 2);
}
