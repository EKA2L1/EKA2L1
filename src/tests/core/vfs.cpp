#include <epoc/vfs.h>
#include <common/path.h>
#include <common/types.h>
#include <gtest/gtest.h>

struct io_scope_guard {
    eka2l1::io_system *io;

    io_scope_guard(eka2l1::io_system &io_sys)
        : io(&io_sys) {
        io->init();

        auto physical_fs = eka2l1::create_physical_filesystem(epocver::epoc9);
        io->add_filesystem(physical_fs);
    }

    ~io_scope_guard() {
        io->shutdown();
    }
};

TEST(vfs, get_physical) {
    eka2l1::io_system io;
    io_scope_guard guard(io);

    // Get physical
    io.mount_physical_path(drive_number::drive_a, eka2l1::drive_media::physical, eka2l1::io_attrib::internal,
        u"drive_a");
    io.mount_physical_path(drive_number::drive_b, eka2l1::drive_media::physical,  eka2l1::io_attrib::internal,
        u"drive_b");

    const auto actual_path_a = io.get_raw_path(u"A:\\Despacito2Leak");
    const auto actual_path_b = io.get_raw_path(u"B:\\Despacito3Leak");

    ASSERT_TRUE(actual_path_a);
    ASSERT_TRUE(actual_path_b);

    ASSERT_EQ(*actual_path_a, std::u16string(u"drive_a") + static_cast<char16_t>(eka2l1::get_separator())
        + u"despacito2leak");

    ASSERT_EQ(*actual_path_b, std::u16string(u"drive_b") + static_cast<char16_t>(eka2l1::get_separator())
        + u"despacito3leak");
}