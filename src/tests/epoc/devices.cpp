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
#include <system/devices.h>

#include <algorithm>
#include <fstream>

using namespace eka2l1;

static bool lists_path(const std::vector<std::string> &paths, const std::string &wanted) {
    return std::find(paths.begin(), paths.end(), wanted) != paths.end();
}

TEST_CASE("per_device_paths_cover_rom_and_shared_drive_state", "devices") {
    const std::vector<std::string> paths = per_device_storage_paths("rm-320");

    // The device's own drive Z and ROM image.
    REQUIRE(lists_path(paths, "drives/z/rm-320/"));
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
            for (const std::string &path : per_device_storage_paths(firmcode)) {
                common::delete_folder(add_path(root, path));
            }
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
