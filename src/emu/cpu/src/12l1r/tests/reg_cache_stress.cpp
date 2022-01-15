#include <catch2/catch.hpp>
#include "test_cpu.h"

TEST_CASE("use_full_regs", "reg_cache_stress") {
    eka2l1::arm::test_env env;

    env.code_ = {
        0xe1a0900d,       // MOV R9, SP
        0xE2490004,       // SUB R0, R9, #4
        0xe8a97cff,       // STMIA R9!, {R0-R7,R10-LR}
        0xE1A00008,       // MOV R0, R8
        0xE1A0100A,       // MOV R1, R10
        0xE0802001,       // ADD R2, R0, R1
        0xE048300A,       // SUB R3, R8, R10
        0xE1A0B103,       // MOV R11, R3, LSL#2
        0xE28BC020,       // ADD R12, R11, #32
        0xE08CC009,       // ADD R12, R12, R9
        0xE083400C,       // ADD R4, R3, R12
        0xEAFFFFFE        // b +#0 (infinite loop)
    };

    env.stack_.resize(104);
    auto core = eka2l1::arm::make_test_cpu(env);

    for (int i = 0; i <= 14; i++) {
        core->set_reg(i, i);
    }

    core->set_reg(13, 0);
    core->set_pc(0);

    core->set_cpsr(0x000001d0);
    core->run(10);

    REQUIRE(env.stack_[0] == 0xFFFFFFFC);
    REQUIRE(env.stack_[1] == 1);
    REQUIRE(env.stack_[2] == 2);
    REQUIRE(env.stack_[3] == 3);
    REQUIRE(env.stack_[4] == 4);
    REQUIRE(env.stack_[5] == 5);
    REQUIRE(env.stack_[6] == 6);
    REQUIRE(env.stack_[7] == 7);
    REQUIRE(env.stack_[8] == 10);
    REQUIRE(env.stack_[9] == 11);
    REQUIRE(env.stack_[10] == 12);
    REQUIRE(env.stack_[11] == 0);
    REQUIRE(env.stack_[12] == 14);

    REQUIRE(core->get_reg(0) == 8);
    REQUIRE(core->get_reg(1) == 10);
    REQUIRE(core->get_reg(2) == 18);
    REQUIRE(core->get_reg(3) == 0xFFFFFFFE);
    REQUIRE(core->get_reg(4) == 0x4A);
    REQUIRE(core->get_reg(5) == 5);
    REQUIRE(core->get_reg(6) == 6);
    REQUIRE(core->get_reg(7) == 7);
    REQUIRE(core->get_reg(8) == 8);
    REQUIRE(core->get_reg(9) == 52);
    REQUIRE(core->get_reg(10) == 10);
    REQUIRE(core->get_reg(11) == 0xFFFFFFF8);
    REQUIRE(core->get_reg(12) == 0x4C);
    REQUIRE(core->get_reg(13) == 0);
    REQUIRE(core->get_reg(14) == 0xE);

    REQUIRE(core->get_cpsr() == 0x000001D0);            // Flags got resetted
}

namespace eka2l1::arm::r12l1 {
    void register_reg_cache_stress_test() {
        return;
    }
}