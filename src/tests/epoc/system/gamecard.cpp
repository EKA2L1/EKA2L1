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

#include <common/fileutils.h>
#include <common/path.h>
#include <config/app_settings.h>
#include <config/config.h>
#include <system/epoc.h>
#include <vfs/vfs.h>

using namespace eka2l1;

namespace {
    constexpr const char *WRAPPED_CARD_PATH = "systemassets//wrappedcard.zip";
    constexpr const char *FLAT_CARD_PATH = "systemassets//flatcard.zip";
    constexpr const char *NO_SYSTEM_CARD_PATH = "systemassets//nosystemcard.zip";
    constexpr const char *BACKUP_SYSTEM_CARD_PATH = "systemassets//backupsystemcard.zip";

    struct card_mount_fixture {
        config::state conf;
        config::app_settings settings;
        system_create_components components;
        std::unique_ptr<eka2l1::system> sys;

        explicit card_mount_fixture(const std::string &cache_root)
            : settings(&conf) {
            common::delete_folder(cache_root);

            components.conf_ = &conf;
            components.settings_ = &settings;
            components.cache_root_ = cache_root;

            sys = std::make_unique<eka2l1::system>(components);
            // startup() registers the physical filesystem used by card mounts.
            sys->startup();
        }

        std::string mounted_path() const {
            std::optional<drive> entry = sys->get_io_system()->get_drive_entry(drive_e);
            return entry.has_value() ? entry->real_path : std::string();
        }
    };
}

TEST_CASE("mount_game_zip_drops_the_folder_wrapping_the_card", "gamecard") {
    card_mount_fixture fixture("gamecardcache_wrapped/");

    REQUIRE(fixture.sys->mount_game_zip(drive_e, drive_media::physical, WRAPPED_CARD_PATH)
        == zip_mount_error_none);

    const std::string root = fixture.mounted_path();
    REQUIRE_FALSE(root.empty());
    REQUIRE(common::is_dir(add_path(root, "System//Apps//6R63//")));
    REQUIRE(common::exists(add_path(root, "Images//cover.txt")));
    REQUIRE_FALSE(common::exists(add_path(root, "System Rush//")));
}

TEST_CASE("mount_game_zip_keeps_a_card_packed_at_the_top", "gamecard") {
    card_mount_fixture fixture("gamecardcache_flat/");

    REQUIRE(fixture.sys->mount_game_zip(drive_e, drive_media::physical, FLAT_CARD_PATH)
        == zip_mount_error_none);

    const std::string root = fixture.mounted_path();
    REQUIRE_FALSE(root.empty());
    REQUIRE(common::exists(add_path(root, "System//Apps//6R63//6R63.APP")));
}

TEST_CASE("mount_game_zip_reads_past_a_wrapper_ending_in_the_root_folder_name", "gamecard") {
    card_mount_fixture fixture("gamecardcache_backupsystem/");

    // BackupSystem must not hide the real System component that follows it.
    REQUIRE(fixture.sys->mount_game_zip(drive_e, drive_media::physical, BACKUP_SYSTEM_CARD_PATH)
        == zip_mount_error_none);

    const std::string root = fixture.mounted_path();
    REQUIRE_FALSE(root.empty());
    REQUIRE(common::exists(add_path(root, "System//Apps//6R63//6R63.APP")));
    REQUIRE(common::exists(add_path(root, "Images//cover.txt")));
}

TEST_CASE("mount_game_zip_refuses_an_archive_without_a_system_folder", "gamecard") {
    card_mount_fixture fixture("gamecardcache_nosystem/");

    REQUIRE(fixture.sys->mount_game_zip(drive_e, drive_media::physical, NO_SYSTEM_CARD_PATH)
        == zip_mount_error_no_system_folder);
    REQUIRE(fixture.mounted_path().empty());
}
