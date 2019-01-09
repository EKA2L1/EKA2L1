#include <gtest/gtest.h>
#include <common/log.h>

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);
    
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();

    return ret;
}
