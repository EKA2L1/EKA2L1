#include <catch2/catch.hpp>
#include "test_cpu.h"

TEST_CASE("simple_invalidation_no_link", "imb_range") {
    eka2l1::arm::test_env env;
    auto core = eka2l1::arm::make_test_cpu(env);

    env.code_ = {
        0xe3a00005,  // mov r0, #5
        0xe3a0100D,  // mov r1, #13
        0xe0812000,  // add r2, r1, r0
        0xeafffffe,  // b +#0 (infinite loop)
    };

    for (int i = 0; i < 15; i++) {
        core->set_reg(i, 0);
    }

    core->set_cpsr(0x000001d0);
    core->run(3);

    REQUIRE(core->get_reg(0) == 5);
    REQUIRE(core->get_reg(1) == 13);
    REQUIRE(core->get_reg(2) == 18);

    env.code_[1] = 0xe3a01007;  // mov r1, #7
    core->imb_range(4, 4);

    core->set_pc(0);
    core->run(3);

    REQUIRE(core->get_reg(0) == 5);
    REQUIRE(core->get_reg(1) == 7);
    REQUIRE(core->get_reg(2) == 12);
}

namespace eka2l1::arm::r12l1 {
    void register_imb_range_test() {
        return;
    }
}