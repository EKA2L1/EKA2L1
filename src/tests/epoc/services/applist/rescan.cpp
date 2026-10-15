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

#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <config/app_settings.h>
#include <config/config.h>
#include <services/applist/applist.h>
#include <system/epoc.h>
#include <vfs/vfs.h>

#include <future>
#include <memory>
#include <mutex>

using namespace eka2l1;

namespace {
    // Holds no files. Notes whether the registration list was locked each time the I/O system
    // asks it whether a file exists: the I/O system holds its own lock then, and mounting a drive
    // takes the two locks in the opposite order.
    class lock_probe_filesystem : public abstract_file_system {
        std::mutex &list_lock_;

    public:
        bool asked_under_list_lock = false;

        explicit lock_probe_filesystem(std::mutex &list_lock)
            : list_lock_(list_lock) {
        }

        bool exists(const std::u16string &) override {
            // Probe from another thread: try_lock on a mutex the calling thread owns is undefined.
            const bool locked = std::async(std::launch::async, [this]() {
                if (!list_lock_.try_lock()) {
                    return true;
                }

                list_lock_.unlock();
                return false;
            }).get();

            asked_under_list_lock = asked_under_list_lock || locked;
            return false;
        }

        bool replace(const std::u16string &, const std::u16string &) override { return false; }
        bool unmount(const drive_number) override { return false; }
        std::unique_ptr<file> open_file(const std::u16string &, const int) override { return nullptr; }
        std::unique_ptr<directory> open_directory(const std::u16string &, epoc::uid_type, const std::uint32_t) override {
            return nullptr;
        }
        std::optional<entry_info> get_entry_info(const std::u16string &) override { return std::nullopt; }
        bool delete_entry(const std::u16string &) override { return false; }
        bool create_directory(const std::u16string &) override { return false; }
        bool create_directories(const std::u16string &) override { return false; }
        std::optional<drive> get_drive_entry(const drive_number) override { return std::nullopt; }
        std::optional<std::u16string> get_raw_path(const std::u16string &) override { return std::nullopt; }
        void validate_for_host() override {}
    };

    struct applist_fixture {
        config::state conf;
        config::app_settings settings;
        system_create_components components;
        std::unique_ptr<eka2l1::system> sys;

        explicit applist_fixture(const std::string &drive_root)
            : settings(&conf) {
            components.conf_ = &conf;
            components.settings_ = &settings;

            sys = std::make_unique<eka2l1::system>(components);
            sys->startup();

            REQUIRE(sys->get_io_system()->mount_physical_path(drive_c, drive_media::physical, io_attrib_internal,
                common::utf8_to_ucs2(drive_root)));
        }
    };
}

TEST_CASE("rescan_checks_whether_files_exist_without_the_registration_lock", "[applist_rescan]") {
    const std::string root = "applistrescan_drive/";
    const std::string apps = add_path(root, "private/10003a3f/apps/");
    const std::string registration = add_path(apps, "sample_reg.rsc");
    const std::string resources = add_path(root, "resource/apps/");

    common::delete_folder(root);
    common::create_directories(apps);
    common::create_directories(resources);
    REQUIRE(common::copy_file("applistassets//sample_reg.rsc", registration, true));

    // The list only keeps an app whose localisable resource file is there too.
    REQUIRE(common::copy_file("applistassets//localised_sample_reg.rsc", add_path(resources, "ITried_0xed3e09d5.rsc"), true));

    applist_fixture fixture(root);
    io_system *io = fixture.sys->get_io_system();

    applist_server applist(fixture.sys.get());
    applist.rescan_registries(io);
    REQUIRE(applist.get_registerations().size() == 1);

    auto probe = std::make_shared<lock_probe_filesystem>(applist.list_access_mut_);
    file_system_inst probe_inst = probe;
    REQUIRE(io->add_filesystem(probe_inst));

    // The file is gone, so the rescan asks every filesystem about it and drops the entry.
    REQUIRE(common::remove(registration));
    applist.rescan_registries(io);

    REQUIRE(applist.get_registerations().empty());
    REQUIRE_FALSE(probe->asked_under_list_lock);

    common::delete_folder(root);
}
