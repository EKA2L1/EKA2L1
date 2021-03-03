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

#include <scripting/instance.h>
#include <scripting/mem.h>

#include <system/epoc.h>
#include <mem/mem.h>
#include <utils/des.h>

namespace eka2l1::scripting {
    uint8_t read_byte(const uint32_t addr) {
        char result = 0;
        get_current_instance()->get_memory_system()->read(addr, &result, 1);

        return result;
    }

    uint16_t read_word(const uint32_t addr) {
        uint16_t result = 0;
        get_current_instance()->get_memory_system()->read(addr, &result, 2);

        return result;
    }

    uint32_t read_dword(const uint32_t addr) {
        uint32_t result = 0;
        get_current_instance()->get_memory_system()->read(addr, &result, 4);

        return result;
    }

    uint64_t read_qword(const uint32_t addr) {
        uint64_t result = 0;
        get_current_instance()->get_memory_system()->read(addr, &result, 8);

        return result;
    }
}

extern "C" {
    EKA2L1_EXPORT std::uint8_t symemu_mem_read_byte(const std::uint32_t addr) {
        return eka2l1::scripting::read_byte(addr);
    }
    
    EKA2L1_EXPORT std::uint16_t symemu_mem_read_word(const std::uint32_t addr) {
        return eka2l1::scripting::read_word(addr);
    }

    EKA2L1_EXPORT std::uint32_t symemu_mem_read_dword(const std::uint32_t addr) {
        return eka2l1::scripting::read_dword(addr);
    }
    
    EKA2L1_EXPORT std::uint64_t symemu_mem_read_qword(const std::uint32_t addr) {
        return eka2l1::scripting::read_qword(addr);
    }
}