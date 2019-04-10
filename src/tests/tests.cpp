#include <common/log.h>
#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

int main(int argc, char **argv) {
    eka2l1::log::setup_log(nullptr);
    int ret = Catch::Session().run(argc, argv);

    return ret;
}
