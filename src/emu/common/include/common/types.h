#pragma once

#include <string>

typedef std::u16string utf16_str;
typedef uint32_t address;

struct uint128_t {
    uint64_t low;
    uint64_t high;
};

enum class prot {
    none = 0,
    read = 1,
    write = 2,
    exec = 3,
    read_write = 4,
    read_exec = 5,
    read_write_exec = 6
};

int translate_protection(prot cprot);
