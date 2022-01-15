#pragma once

#include <cpu/12l1r/arm_12l1r.h>
#include <cpu/12l1r/exclusive_monitor.h>

#include <vector>

namespace eka2l1::arm {
    struct test_env {
        std::vector<std::uint32_t> code_;
        std::vector<std::uint32_t> stack_;
        r12l1::exclusive_monitor monitor_;

        explicit test_env();
    };

    std::unique_ptr<r12l1_core> make_test_cpu(test_env &environment);
}