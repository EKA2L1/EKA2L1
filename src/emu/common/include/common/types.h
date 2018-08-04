/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <string>

#define FOUND_STR(x) x != std::string::npos
#define INVALID_HANDLE 0xFFFFFFFF

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

// This can be changed manually
enum class epocver {
    epocu6,
    epoc6,    // Epoc 6.0
    epoc93,   // Epoc 9.3
    epoc9,    // Epoc 9.4
    epoc10
};

int translate_protection(prot cprot);

