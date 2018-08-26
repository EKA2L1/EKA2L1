#pragma once

#include <cstdint>

namespace eka2l1::scripting {
    char read_byte(const uint32_t addr);
    short read_short(const uint32_t addr);
    uint32_t read_word(const uint32_t addr);
    uint64_t read_qword(const uint32_t addr);
}