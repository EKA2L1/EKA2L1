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
#include <config/config.h>
#include <system/devices.h>
#include <system/software.h>

#include <algorithm>
#include <fstream>

using namespace eka2l1;

static bool lists_path(const std::vector<std::string> &paths, const std::string &wanted) {
    return std::find(paths.begin(), paths.end(), wanted) != paths.end();
}

TEST_CASE("per_device_paths_cover_rom_and_shared_drive_state", "devices") {
    const std::vector<std::string> paths = per_device_storage_paths("rm-320");

    // The device's own drive Z, ROM image and isolated drives.
    REQUIRE(lists_path(paths, "drives/z/rm-320/"));
    REQUIRE(lists_path(paths, "drives/rm-320/"));
    REQUIRE(lists_path(paths, "roms/rm-320/"));

    // What the servers leave on the drives every device shares. Drive C is where
    // these actually land today, but a repository resides on whichever writable
    // drive it was loaded from, so D and E have to be covered too.
    REQUIRE(lists_path(paths, "drives/c/private/10202be9/persists/rm-320/"));
    REQUIRE(lists_path(paths, "drives/d/private/10202be9/persists/rm-320/"));
    REQUIRE(lists_path(paths, "drives/e/private/10202be9/persists/rm-320/"));
    REQUIRE(lists_path(paths, "drives/c/private/1000484b/mail2/rm-320/"));
    REQUIRE(lists_path(paths, "drives/c/system/mail/rm-320/"));
    REQUIRE(lists_path(paths, "drives/c/system/mtm/rm-320/"));
}

TEST_CASE("device_drive_folder_follows_isolation", "devices") {
    REQUIRE(device_drive_folder("RM-707", false, drive_c) == "drives/c/");
    REQUIRE(device_drive_folder("RM-707", false, drive_e) == "drives/e/");
    REQUIRE(device_drive_folder("RM-707", true, drive_c) == "drives/rm-707/c/");
    REQUIRE(device_drive_folder("RM-707", true, drive_d) == "drives/rm-707/d/");
    REQUIRE(device_drive_folder("RM-707", true, drive_e) == "drives/rm-707/e/");
}

TEST_CASE("per_device_paths_lowercase_the_firmware_code", "devices") {
    // devices.yml stores the code as the ROM spells it ("RM-320"), while everything
    // written to disk goes through common::lowercase_string.
    const std::vector<std::string> paths = per_device_storage_paths("RM-320");

    REQUIRE(lists_path(paths, "drives/z/rm-320/"));
    REQUIRE(lists_path(paths, "drives/c/private/10202be9/persists/rm-320/"));

    for (const std::string &path : paths) {
        REQUIRE(path.find("RM-320") == std::string::npos);
        REQUIRE(path.back() == '/');
    }
}

namespace {
    // A throwaway data root holding state for two devices, plus state shared by both.
    struct storage_test_env {
        std::string root;

        explicit storage_test_env(const std::string &name)
            : root(add_path("devicestestenv", name + eka2l1::get_separator())) {
            common::delete_folder(root);
            common::create_directories(root);
        }

        ~storage_test_env() {
            common::delete_folder(root);
        }

        void write_file(const std::string &relative) {
            const std::string full = add_path(root, relative);
            common::create_directories(eka2l1::file_directory(full));

            std::ofstream stream(full, std::ios::binary);
            REQUIRE(stream.good());
            stream << "data";
        }

        bool has(const std::string &relative) const {
            return common::exists(add_path(root, relative));
        }

        void delete_device_state(const std::string &firmcode) {
            delete_device_storage(root, firmcode);
        }
    };
}

TEST_CASE("deleting_a_device_leaves_no_state_behind", "devices") {
    storage_test_env env("delete_one_device");

    env.write_file("drives/z/rm-320/sys/bin/euser.dll");
    env.write_file("roms/rm-320/SYM.ROM");
    env.write_file("drives/c/private/10202be9/persists/rm-320/101f876f.cre");
    env.write_file("drives/c/private/1000484b/mail2/rm-320/messaging.db");
    env.write_file("drives/e/private/10202be9/persists/rm-320/101f876f.cre");

    // A second device, and state that belongs to no device in particular. The
    // deletion has no business touching either.
    env.write_file("drives/z/rm-409/sys/bin/euser.dll");
    env.write_file("roms/rm-409/SYM.ROM");
    env.write_file("drives/c/private/10202be9/persists/rm-409/101f876f.cre");
    env.write_file("drives/c/private/10202be9/20008bb7.txt");
    env.write_file("drives/c/private/1000484b/mtm registry v2");
    env.write_file("drives/e/system/apps/mygame/mygame.exe");

    env.delete_device_state("rm-320");

    REQUIRE_FALSE(env.has("drives/z/rm-320/sys/bin/euser.dll"));
    REQUIRE_FALSE(env.has("roms/rm-320/SYM.ROM"));
    REQUIRE_FALSE(env.has("drives/c/private/10202be9/persists/rm-320/101f876f.cre"));
    REQUIRE_FALSE(env.has("drives/c/private/1000484b/mail2/rm-320/messaging.db"));
    REQUIRE_FALSE(env.has("drives/e/private/10202be9/persists/rm-320/101f876f.cre"));

    REQUIRE(env.has("drives/z/rm-409/sys/bin/euser.dll"));
    REQUIRE(env.has("roms/rm-409/SYM.ROM"));
    REQUIRE(env.has("drives/c/private/10202be9/persists/rm-409/101f876f.cre"));
    REQUIRE(env.has("drives/c/private/10202be9/20008bb7.txt"));
    REQUIRE(env.has("drives/c/private/1000484b/mtm registry v2"));
    REQUIRE(env.has("drives/e/system/apps/mygame/mygame.exe"));
}

TEST_CASE("deleting_a_device_that_wrote_nothing_is_fine", "devices") {
    storage_test_env env("delete_untouched_device");

    env.write_file("drives/z/rm-320/sys/bin/euser.dll");

    // Only drive Z exists: a device installed but never booted has no repository
    // persists or message store yet, and the missing folders must not upset it.
    env.delete_device_state("rm-320");

    REQUIRE_FALSE(env.has("drives/z/rm-320/sys/bin/euser.dll"));
}

TEST_CASE("Series 90 markers select the OS 7.0s contracts", "devices") {
    for (const std::string marker : { "series90v10.sis", "series90v11.sis" }) {
        storage_test_env env(marker);
        env.write_file("system/install/" + marker);
        REQUIRE(loader::determine_rpkg_symbian_version(env.root) == epocver::epoc7);
    }
}

TEST_CASE("deleting_an_isolated_device_takes_its_drives", "devices") {
    storage_test_env env("delete_isolated_device");

    env.write_file("drives/z/rm-707/sys/bin/euser.dll");
    env.write_file("drives/rm-707/c/private/10202be9/persists/101f876f.cre");
    env.write_file("drives/rm-707/e/system/apps/mygame/mygame.exe");
    env.write_file("drives/rm-409/e/system/apps/othergame/othergame.exe");
    env.write_file("drives/e/system/apps/sharedgame/sharedgame.exe");

    env.delete_device_state("rm-707");

    REQUIRE_FALSE(env.has("drives/rm-707/c/private/10202be9/persists/101f876f.cre"));
    REQUIRE_FALSE(env.has("drives/rm-707/e/system/apps/mygame/mygame.exe"));

    REQUIRE(env.has("drives/rm-409/e/system/apps/othergame/othergame.exe"));
    REQUIRE(env.has("drives/e/system/apps/sharedgame/sharedgame.exe"));
}

TEST_CASE("isolated_drives_persist_in_devices_yml", "devices") {
    storage_test_env env("isolated_drives_yml");

    config::state conf;
    conf.storage = env.root;

    {
        device_manager manager(&conf);
        REQUIRE(manager.add_new_device("RM-707", "X7-00", "Nokia", epocver::epoc10, 0, true) == add_device_none);
        REQUIRE(manager.add_new_device("RM-409", "5320", "Nokia", epocver::epoc94, 0, false) == add_device_none);
        manager.save_devices();
    }

    // The folder is how a rescan tells the device apart, so it exists from the start.
    REQUIRE(common::is_dir(add_path(env.root, "drives/rm-707/")));
    REQUIRE_FALSE(common::exists(add_path(env.root, "drives/rm-409/")));

    device_manager reloaded(&conf);
    REQUIRE(reloaded.total() == 2);
    REQUIRE(reloaded.get("RM-707")->isolated_drives);
    REQUIRE_FALSE(reloaded.get("RM-409")->isolated_drives);
}

TEST_CASE("devices_yml_without_isolation_key_is_shared", "devices") {
    storage_test_env env("legacy_devices_yml");

    {
        std::ofstream stream(add_path(env.root, "devices.yml"), std::ios::binary | std::ios::trunc);
        stream << "RM-409:\n  platver: epoc94\n  manufacturer: Nokia\n  firmcode: RM-409\n  model: \"5320\"\n  machine-uid: 536926810\n";
    }

    config::state conf;
    conf.storage = env.root;

    device_manager manager(&conf);
    REQUIRE(manager.total() == 1);
    REQUIRE_FALSE(manager.get("RM-409")->isolated_drives);
}

TEST_CASE("device_marked_for_deletion_goes_on_next_load", "devices") {
    storage_test_env env("marked_for_deletion");

    config::state conf;
    conf.storage = env.root;

    {
        device_manager manager(&conf);
        REQUIRE(manager.add_new_device("RM-707", "X7-00", "Nokia", epocver::epoc10, 0, true) == add_device_none);
        REQUIRE(manager.add_new_device("RM-409", "5320", "Nokia", epocver::epoc94, 0, false) == add_device_none);
        manager.save_devices();

        env.write_file("drives/z/rm-707/sys/bin/euser.dll");
        env.write_file("roms/rm-707/SYM.ROM");
        env.write_file("drives/rm-707/e/sys/bin/game.exe");
        env.write_file("drives/z/rm-409/sys/bin/euser.dll");

        REQUIRE(manager.mark_for_deletion("RM-707"));
        REQUIRE_FALSE(manager.mark_for_deletion("RM-000"));

        // Nothing goes while the device may still be running.
        REQUIRE(env.has("drives/rm-707/e/sys/bin/game.exe"));
    }

    device_manager reloaded(&conf);
    REQUIRE(reloaded.total() == 1);
    REQUIRE(reloaded.get("RM-409"));

    REQUIRE_FALSE(env.has("drives/z/rm-707/sys/bin/euser.dll"));
    REQUIRE_FALSE(env.has("roms/rm-707/SYM.ROM"));
    REQUIRE_FALSE(env.has("drives/rm-707/"));
    REQUIRE(env.has("drives/z/rm-409/sys/bin/euser.dll"));

    // The reload rewrote devices.yml without the device.
    device_manager again(&conf);
    REQUIRE(again.total() == 1);
}
