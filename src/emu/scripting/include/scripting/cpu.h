#pragma once

#include <cstdint>

namespace eka2l1::scripting {
    struct cpu {
        static std::uint32_t get_register(const int index);
        static void set_register(const int index, const std::uint32_t value);
        static std::uint32_t get_cpsr();
        static std::uint32_t get_pc();
        static std::uint32_t get_sp();
        static std::uint32_t get_lr();
    };
}