#pragma once

#include <cstdint>

namespace eka2l1::scripting {
    struct cpu {
        static uint32_t get_register(const int index);
        static uint32_t get_cpsr();
        static uint32_t get_pc();
        static uint32_t get_sp();
        static uint32_t get_lr();
    };
}