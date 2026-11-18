#include <catch2/catch.hpp>
#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/crypt.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <common/types.h>
#include <vfs/vfs.h>

#include <fstream>

TEST_CASE("uid_directory_listing_preserves_directories_and_untyped_files", "vfs") {
    const std::string root = "vfs_uid_listing";
    eka2l1::common::delete_folder(eka2l1::host_path{root});
    eka2l1::common::create_directories(eka2l1::host_path{eka2l1::add_path(root, "Free Demo Levels")});
    {
        std::ofstream empty(eka2l1::add_path(root, "empty.dat"), std::ios::binary);
        std::ofstream typed(eka2l1::add_path(root, "typed.dat"), std::ios::binary);
        const eka2l1::epoc::uid_type uid{ 1, 2, 3 };
        typed.write(reinterpret_cast<const char *>(&uid), sizeof(uid));
        const auto checksum = eka2l1::crypt::calculate_checked_uid_checksum(reinterpret_cast<const std::uint32_t *>(&uid));
        typed.write(reinterpret_cast<const char *>(&checksum), sizeof(checksum));
        std::ofstream short_file(eka2l1::add_path(root, "short.dat"), std::ios::binary);
        short_file.write(reinterpret_cast<const char *>(&uid), sizeof(uid));
        std::ofstream corrupt(eka2l1::add_path(root, "corrupt.dat"), std::ios::binary);
        corrupt.write(reinterpret_cast<const char *>(&uid), sizeof(uid));
        const std::uint32_t invalid_checksum = checksum ^ 1;
        corrupt.write(reinterpret_cast<const char *>(&invalid_checksum), sizeof(invalid_checksum));
    }

    eka2l1::io_system io;
    auto fs = eka2l1::create_physical_filesystem(epocver::epoc70, "");
    io.add_filesystem(fs);
    REQUIRE(io.mount_physical_path(drive_number::drive_a, drive_media::physical, io_attrib_internal,
        eka2l1::host_path{root}));

    const auto attributes = io_attrib_include_file | io_attrib_include_dir | io_attrib_allow_uid;
    auto dir = io.open_dir(eka2l1::guest_path{u"A:\\"}, {}, attributes);
    REQUIRE(dir);
    std::vector<std::string> names;
    while (auto entry = dir->get_next_entry()) {
        names.push_back(entry->name.utf8());
    }
    std::sort(names.begin(), names.end());
    REQUIRE(names == std::vector<std::string>{ "Free Demo Levels", "corrupt.dat", "empty.dat", "short.dat", "typed.dat" });

    dir = io.open_dir(eka2l1::guest_path{u"A:\\"}, { 1, 2, 3 }, attributes);
    REQUIRE(dir);
    names.clear();
    while (auto entry = dir->get_next_entry()) {
        names.push_back(entry->name.utf8());
    }
    std::sort(names.begin(), names.end());
    REQUIRE(names == std::vector<std::string>{ "Free Demo Levels", "typed.dat" });
    eka2l1::common::delete_folder(eka2l1::host_path{root});
}

struct io_scope_guard {
    eka2l1::io_system *io;

    io_scope_guard(eka2l1::io_system &io_sys)
        : io(&io_sys) {
        auto physical_fs = eka2l1::create_physical_filesystem(epocver::epoc94, "");
        io->add_filesystem(physical_fs);
    }

    ~io_scope_guard() {
    }
};

TEST_CASE("drive_remount_after_notification_removal", "vfs") {
    eka2l1::io_system io;
    io_scope_guard guard(io);
    int removed_calls = 0;
    int active_calls = 0;
    const auto removed = io.register_drive_change_notify([&](void *, drive_number, eka2l1::drive_action) {
        ++removed_calls;
    }, nullptr);
    const auto active = io.register_drive_change_notify([&](void *, drive_number drv, eka2l1::drive_action) {
        REQUIRE(drv == drive_c);
        ++active_calls;
    }, nullptr);

    REQUIRE(io.mount_physical_path(drive_c, drive_media::physical, io_attrib_internal, eka2l1::host_path{"."}));
    REQUIRE(removed_calls == 1);
    REQUIRE(active_calls == 1);
    REQUIRE(io.remove_drive_change_notify(removed));
    REQUIRE(io.unmount(drive_c));
    REQUIRE(io.mount_physical_path(drive_c, drive_media::physical, io_attrib_internal, eka2l1::host_path{"."}));
    REQUIRE(removed_calls == 1);
    REQUIRE(active_calls == 3);

    REQUIRE(io.remove_drive_change_notify(active));
    REQUIRE(io.unmount(drive_c));
    REQUIRE(io.mount_physical_path(drive_c, drive_media::physical, io_attrib_internal, eka2l1::host_path{"."}));
    REQUIRE(active_calls == 3);
}

TEST_CASE("get_physical", "vfs") {
    eka2l1::io_system io;
    io_scope_guard guard(io);

    // Get physical
    io.mount_physical_path(drive_number::drive_a, drive_media::physical, io_attrib_internal,
        eka2l1::host_path{"drive_a"});
    io.mount_physical_path(drive_number::drive_b, drive_media::physical, io_attrib_internal,
        eka2l1::host_path{"drive_b"});

    const auto actual_path_a = io.get_raw_path(eka2l1::guest_path{u"A:\\Despacito2Leak"});
    const auto actual_path_b = io.get_raw_path(eka2l1::guest_path{u"B:\\Despacito3Leak"});

    REQUIRE(actual_path_a);
    REQUIRE(actual_path_b);

    REQUIRE(*actual_path_a == (eka2l1::host_path("drive_a") / "Despacito2Leak"));

    REQUIRE(*actual_path_b == (eka2l1::host_path("drive_b") / "Despacito3Leak"));
}

TEST_CASE("open_file_the_host_refuses", "vfs") {
    eka2l1::io_system io;
    io_scope_guard guard(io);

    io.mount_physical_path(drive_number::drive_a, drive_media::physical, io_attrib_internal,
        eka2l1::host_path{"."});

    // The parent directory does not exist, so the host refuses the handle.
    // RFile::Replace answers KErrPathNotFound there; the file server never hands
    // back a file object with nothing behind it.
    REQUIRE(io.open_file(eka2l1::guest_path{u"A:\\NoSuchFolder\\NotHere.txt"}, WRITE_MODE | BIN_MODE) == nullptr);
}

TEST_CASE("physical_filesystem_is_case_insensitive_but_preserves_entry_case", "vfs") {
    const std::string root = "vfs_case_preserving";
    eka2l1::common::delete_folder(eka2l1::host_path{root});
    eka2l1::common::create_directories(eka2l1::host_path{eka2l1::add_path(root, "GameData")});

    {
        std::ofstream settings(eka2l1::add_path(root, "GameData/Settings.dat"), std::ios::binary);
        settings.put('x');
    }

    eka2l1::io_system io;
    io_scope_guard guard(io);
    REQUIRE(io.mount_physical_path(drive_number::drive_a, drive_media::physical, io_attrib_internal,
        eka2l1::host_path{root}));

    const auto resolved = io.get_raw_path(eka2l1::guest_path{u"A:\\gamedata\\settings.dat"});
    REQUIRE(resolved);
    REQUIRE(eka2l1::common::exists(eka2l1::host_path{eka2l1::common::ucs2_to_utf8(*resolved)}));

    auto dir = io.open_dir(eka2l1::guest_path{u"A:\\GAMEDATA\\*.DAT"}, {}, io_attrib_include_file);
    REQUIRE(dir);
    const auto entry = dir->get_next_entry();
    REQUIRE(entry);
    REQUIRE(entry->name.utf16() == u"Settings.dat");

    const auto new_file = io.get_raw_path(eka2l1::guest_path{u"A:\\gamedata\\NewSave.Dat"});
    REQUIRE(new_file);
    REQUIRE(new_file->filename().utf8() == "NewSave.Dat");

    auto save = io.open_file(eka2l1::guest_path{u"a:/GAMEDATA/NewSave.Dat"}, WRITE_MODE | BIN_MODE);
    REQUIRE(save);
    const char content[] = "saved";
    REQUIRE(save->write_file(content, 1, sizeof(content)) == sizeof(content));
    save.reset();
    REQUIRE(io.exist(eka2l1::guest_path{u"A:\\gamedata\\NEWSAVE.DAT"}));
    REQUIRE(io.rename(eka2l1::guest_path{u"a:/gamedata/newsave.dat"},
        eka2l1::guest_path{u"A:\\GAMEDATA\\Renamed.Dat"}));
    REQUIRE_FALSE(io.exist(eka2l1::guest_path{u"A:\\GameData\\NewSave.Dat"}));
    REQUIRE(eka2l1::common::exists(eka2l1::host_path{root}.join("GameData/Renamed.Dat")));
    REQUIRE(io.delete_entry(eka2l1::guest_path{u"a:/gamedata/RENAMED.DAT"}));
    REQUIRE_FALSE(io.exist(eka2l1::guest_path{u"A:\\GameData\\Renamed.Dat"}));

    eka2l1::common::delete_folder(eka2l1::host_path{root});
}
