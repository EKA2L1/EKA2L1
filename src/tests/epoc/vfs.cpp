#include <catch2/catch.hpp>
#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/path.h>
#include <common/types.h>
#include <vfs/vfs.h>

#include <fstream>

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

TEST_CASE("get_physical", "vfs") {
    eka2l1::io_system io;
    io_scope_guard guard(io);

    // Get physical
    io.mount_physical_path(drive_number::drive_a, drive_media::physical, io_attrib_internal,
        u"drive_a");
    io.mount_physical_path(drive_number::drive_b, drive_media::physical, io_attrib_internal,
        u"drive_b");

    const auto actual_path_a = io.get_raw_path(u"A:\\Despacito2Leak");
    const auto actual_path_b = io.get_raw_path(u"B:\\Despacito3Leak");

    REQUIRE(actual_path_a);
    REQUIRE(actual_path_b);

    REQUIRE(eka2l1::common::compare_ignore_case(*actual_path_a, std::u16string(u"drive_a") + static_cast<char16_t>(eka2l1::get_separator()) + u"despacito2leak") == 0);

    REQUIRE(eka2l1::common::compare_ignore_case(*actual_path_b, std::u16string(u"drive_b") + static_cast<char16_t>(eka2l1::get_separator()) + u"despacito3leak") == 0);
}

TEST_CASE("open_file_the_host_refuses", "vfs") {
    eka2l1::io_system io;
    io_scope_guard guard(io);

    io.mount_physical_path(drive_number::drive_a, drive_media::physical, io_attrib_internal,
        u".");

    // The parent directory does not exist, so the host refuses the handle.
    // RFile::Replace answers KErrPathNotFound there; the file server never hands
    // back a file object with nothing behind it.
    REQUIRE(io.open_file(u"A:\\NoSuchFolder\\NotHere.txt", WRITE_MODE | BIN_MODE) == nullptr);
}

TEST_CASE("physical_filesystem_is_case_insensitive_but_preserves_entry_case", "vfs") {
    const std::string root = "vfs_case_preserving";
    eka2l1::common::delete_folder(root);
    eka2l1::common::create_directories(eka2l1::add_path(root, "GameData"));

    {
        std::ofstream settings(eka2l1::add_path(root, "GameData/Settings.dat"), std::ios::binary);
        settings.put('x');
    }

    eka2l1::io_system io;
    io_scope_guard guard(io);
    REQUIRE(io.mount_physical_path(drive_number::drive_a, drive_media::physical, io_attrib_internal,
        eka2l1::common::utf8_to_ucs2(root)));

    const auto resolved = io.get_raw_path(u"A:\\gamedata\\settings.dat");
    REQUIRE(resolved);
    REQUIRE(eka2l1::common::exists(eka2l1::common::ucs2_to_utf8(*resolved)));

    auto dir = io.open_dir(u"A:\\GAMEDATA\\*.DAT", {}, io_attrib_include_file);
    REQUIRE(dir);
    const auto entry = dir->get_next_entry();
    REQUIRE(entry);
    REQUIRE(entry->name == "Settings.dat");

    const auto new_file = io.get_raw_path(u"A:\\gamedata\\NewSave.Dat");
    REQUIRE(new_file);
    REQUIRE(eka2l1::filename(*new_file) == u"NewSave.Dat");

    eka2l1::common::delete_folder(root);
}
