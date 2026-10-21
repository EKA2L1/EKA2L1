#pragma once

#include <cstdint>

namespace eka2l1::scripting {
    uint8_t read_byte(const uint32_t addr);
    uint16_t read_word(const uint32_t addr);
    uint32_t read_dword(const uint32_t addr);
    uint64_t read_qword(const uint32_t addr);

    int32_t write_byte(const uint32_t addr, const uint8_t data);
    int32_t write_word(const uint32_t addr, const uint16_t data);
    int32_t write_dword(const uint32_t addr, const uint32_t data);
    int32_t write_qword(const uint32_t addr, const uint64_t data);
}