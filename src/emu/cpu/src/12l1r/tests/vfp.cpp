#include <catch2/catch.hpp>
#include "test_cpu.h"

TEST_CASE("load_chain_single_double", "vfp") {
    eka2l1::arm::test_env env;
    auto core = eka2l1::arm::make_test_cpu(env);

    env.code_ = {
        0xec9c4a0f,  // vldmida ip, {s8-s22}
        0xeaffffff,  // b +#4
        0xe1a0000c,  // mov r0, ip
        0xeafffffe,  // b +#0 (infinite loop)

        // Random data
        0,
        100,
        0x40490FDB,
        0x408487ED,
        0x43010000,
        0x4887A232,
        0x4AA98AC6,
        0x4ACB7354,
        0x4AD98F2E,
        0x444DBD71,
        0x41100000,
        0x4121C28F,
        0x42E2AA7F,
        0x42F3DBD9,
        0x4306E46E,
        0x41600000,
        15
    };

    for (int i = 0; i < 15; i++) {
        core->set_reg(i, 0);
    }

    core->set_cpsr(0x000001d0);
    core->set_reg(12, 24);
    core->run(4);

    // Should not change
    REQUIRE(core->get_reg(12) == 24);
    REQUIRE(core->get_reg(0) == 24);

    for (std::size_t i = 0; i < 15; i++) {
        REQUIRE(core->get_vfp(8 + i) == env.code_[6 + i]);
    }
}

namespace eka2l1::arm::r12l1 {
    void register_vfp_tests() {
        return;
    }
}