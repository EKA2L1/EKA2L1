#include <catch2/catch.hpp>
#include "test_cpu.h"

TEST_CASE("load_near_page_crossing_with_no_tlb_miss", "mem_stress") {
    static constexpr std::uint32_t THE_PAGE_SIZE = eka2l1::arm::test_env_tlb::ENV_PAGE_SIZE;
    eka2l1::arm::test_env_tlb env(THE_PAGE_SIZE * 2);

    env.code_ = {
        0xe8b14ff1,     // ldmia r1!, { r0, r4-r11, lr }
        0xe0810005,     // add r0, r1, r5
        0xeafffffe,     // b +#0 (infinite loop)
    };

    auto core = eka2l1::arm::make_test_cpu(env);

    env.fill_tlb(*core);
    core->set_reg(0, 1000);
    core->set_reg(1, THE_PAGE_SIZE - 16);
    for (std::uint32_t addr = THE_PAGE_SIZE - 16, i = 0; i < 10; i++, addr += 4) {
        *reinterpret_cast<std::uint32_t*>(env.pointer(addr)) = i * 10;
    }

    core->set_pc(0);
    core->set_cpsr(0x000001d0);
    core->run(2);

    REQUIRE(core->get_reg(1) == THE_PAGE_SIZE + 24);
    REQUIRE(core->get_reg(4) == 10);
    REQUIRE(core->get_reg(5) == 20);
    REQUIRE(core->get_reg(6) == 30);
    REQUIRE(core->get_reg(7) == 40);
    REQUIRE(core->get_reg(8) == 50);
    REQUIRE(core->get_reg(9) == 60);
    REQUIRE(core->get_reg(10) == 70);
    REQUIRE(core->get_reg(11) == 80);
    REQUIRE(core->get_reg(14) == 90);
    REQUIRE(core->get_reg(0) == THE_PAGE_SIZE + 44);
    REQUIRE(core->get_cpsr() == 0x000001D0);            // Flags got resetted
}

TEST_CASE("store_near_page_crossing_with_no_tlb_miss", "mem_stress") {
    static constexpr std::uint32_t THE_PAGE_SIZE = eka2l1::arm::test_env_tlb::ENV_PAGE_SIZE;
    eka2l1::arm::test_env_tlb env(THE_PAGE_SIZE * 2);

    env.code_ = {
        0xe92d4ff1,     // stmdb sp!, { r0, r4-r11, lr }
        0xe08d0005,     // add r0, sp, r5
        0xeafffffe,     // b +#0 (infinite loop)
    };

    auto core = eka2l1::arm::make_test_cpu(env);

    env.fill_tlb(*core);
    core->set_reg(0, 1000);
    core->set_reg(13, THE_PAGE_SIZE + 24);

    for (std::uint32_t i = 4; i <= 11; i++) {
        core->set_reg(i, i * 4);
    }

    core->set_reg(14, 2000);

    core->set_pc(0);
    core->set_cpsr(0x000001d0);
    core->run(2);

#define REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(offset, value) \
    REQUIRE(*reinterpret_cast<std::uint32_t*>(env.pointer(PAGE_SIZE - 16 + offset)) == value)

    REQUIRE(core->get_reg(13) == THE_PAGE_SIZE - 16);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(36, 2000);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(32, 44);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(28, 40);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(24, 36);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(20, 32);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(16, 28);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(12, 24);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(8, 20);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(4, 16);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(0, 1000);

#undef REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS

    REQUIRE(core->get_reg(0) == THE_PAGE_SIZE - 16 + 20);
    REQUIRE(core->get_cpsr() == 0x000001D0);
}

TEST_CASE("store_da_near_page_crossing_with_no_tlb_miss", "mem_stress") {
    // Same data set but this time decrease after
    static constexpr std::uint32_t THE_PAGE_SIZE = eka2l1::arm::test_env_tlb::ENV_PAGE_SIZE;
    eka2l1::arm::test_env_tlb env(THE_PAGE_SIZE * 2);

    env.code_ = {
        0xe82d4ff1,     // stmda sp!, { r0, r4-r11, lr }
        0xe08d0005,     // add r0, sp, r5
        0xeafffffe,     // b +#0 (infinite loop)
    };

    auto core = eka2l1::arm::make_test_cpu(env);

    env.fill_tlb(*core);
    core->set_reg(0, 1000);
    core->set_reg(13, THE_PAGE_SIZE + 24);

    for (std::uint32_t i = 4; i <= 11; i++) {
        core->set_reg(i, i * 4);
    }

    core->set_reg(14, 2000);

    core->set_pc(0);
    core->set_cpsr(0x000001d0);
    core->run(2);

    #define REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(offset, value) \
        REQUIRE(*reinterpret_cast<std::uint32_t*>(env.pointer(PAGE_SIZE - 12 + offset)) == value)

    REQUIRE(core->get_reg(13) == THE_PAGE_SIZE - 16);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(36, 2000);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(32, 44);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(28, 40);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(24, 36);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(20, 32);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(16, 28);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(12, 24);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(8, 20);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(4, 16);
    REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS(0, 1000);

    #undef REQUIRE_POINTER_AT_CHAIN_OFFSET_EQUALS

    REQUIRE(core->get_reg(0) == THE_PAGE_SIZE - 16 + 20);
    REQUIRE(core->get_cpsr() == 0x000001D0);
}

namespace eka2l1::arm::r12l1 {
    void register_mem_stress_tests() {
        return;
    }
}