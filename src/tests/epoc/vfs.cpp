#include <catch2/catch.hpp>
#include <common/algorithm.h>
#include <common/path.h>
#include <common/types.h>
#include <epoc/vfs.h>

struct io_scope_guard {
    eka2l1::io_system *io;

    io_scope_guard(eka2l1::io_system &io_sys)
        : io(&io_sys) {
        io->init();

        auto physical_fs = eka2l1::create_physical_filesystem(epocver::epoc94, "");
        io->add_filesystem(physical_fs);
    }

    ~io_scope_guard() {
        io->shutdown();
    }
};

TEST_CASE("get_physical", "vfs") {
    eka2l1::io_system io;
    io_scope_guard guard(io);

    // Get physical
    io.mount_physical_path(drive_number::drive_a, drive_media::physical, io_attrib::internal,
        u"drive_a");
    io.mount_physical_path(drive_number::drive_b, drive_media::physical, io_attrib::internal,
        u"drive_b");

    const auto actual_path_a = io.get_raw_path(u"A:\\Despacito2Leak");
    const auto actual_path_b = io.get_raw_path(u"B:\\Despacito3Leak");

    REQUIRE(actual_path_a);
    REQUIRE(actual_path_b);

    REQUIRE(eka2l1::common::compare_ignore_case(*actual_path_a, std::u16string(u"drive_a") + static_cast<char16_t>(eka2l1::get_separator()) 
        + u"despacito2leak") == 0);

    REQUIRE(eka2l1::common::compare_ignore_case(*actual_path_b, std::u16string(u"drive_b") + static_cast<char16_t>(eka2l1::get_separator()) 
        + u"despacito3leak") == 0);
}
