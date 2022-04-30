#pragma once

#include <cpu/12l1r/arm_12l1r.h>
#include <cpu/12l1r/exclusive_monitor.h>
#include <cpu/12l1r/tlb.h>

#include <vector>

namespace eka2l1::arm {
    struct test_env {
        std::vector<std::uint32_t> code_;
        std::vector<std::uint32_t> stack_;
        r12l1::exclusive_monitor monitor_;

        explicit test_env();
    };

    struct test_env_tlb {
        enum {
            ENV_PAGE_BITS = 12,
            ENV_PAGE_SIZE = 1 << ENV_PAGE_BITS
        };

        std::vector<std::uint32_t> code_;
        std::vector<std::vector<std::uint8_t>> pages_;
        r12l1::exclusive_monitor monitor_;

        explicit test_env_tlb(const std::uint32_t mem_size);
        void fill_tlb(r12l1_core &core);

        std::uint8_t *pointer(const std::uint32_t addr);
    };

    std::unique_ptr<r12l1_core> make_test_cpu(test_env &environment);
    std::unique_ptr<r12l1_core> make_test_cpu(test_env_tlb &env_tlb);
}