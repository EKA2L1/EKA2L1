#include <core/vfs.h>
#include <gtest/gtest.h>

struct io_scope_guard {
    eka2l1::io_system *io;

    io_scope_guard(eka2l1::io_system &io_sys)
        : io(&io_sys) {
        io->init(nullptr, epocver::epoc9);
    }

    ~io_scope_guard() {
        io->shutdown();
    }
};

TEST(vfs, get_physical) {
    eka2l1::io_system io;
    io_scope_guard guard(io);

    // Get physical
    io.mount(drive_number::drive_a, eka2l1::drive_media::physical, "drive_a");
    io.mount(drive_number::drive_b, eka2l1::drive_media::physical, "drive_b");

    const std::string actual_path_a = io.get("A:\\Despacito2Leak");
    const std::string actual_path_b = io.get("B:\\Despacito3Leak");

    ASSERT_EQ(actual_path_a, "drive_a\\despacito2leak");
    ASSERT_EQ(actual_path_b, "drive_b\\despacito3leak");
}