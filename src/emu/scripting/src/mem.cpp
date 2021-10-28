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

#include <mem/mem.h>

#include <system/epoc.h>
#include <utils/des.h>

#include <kernel/kernel.h>
#include <kernel/process.h>

namespace eka2l1::scripting {
    template <typename T>
    T read_integer_type(const std::uint32_t addr) {
        kernel::process *process = get_current_instance()->get_kernel_system()->crr_process();
        if (process) {
            T *ptr = reinterpret_cast<T*>(process->get_ptr_on_addr_space(addr));
            if (ptr) {
                return *ptr;
            }
        }

        return 0;
    }

    template <typename T>
    std::int32_t write_integer_type(const std::uint32_t addr, const T data) {
        kernel::process *process = get_current_instance()->get_kernel_system()->crr_process();
        if (process) {
            T *ptr = reinterpret_cast<T*>(process->get_ptr_on_addr_space(addr));
            if (ptr) {
                *ptr = data;
                return 1;
            }
        }

        return 0;
    }

    std::uint8_t read_byte(const std::uint32_t addr) {
        return read_integer_type<std::uint8_t>(addr);
    }

    std:: uint16_t read_word(const std::uint32_t addr) {
       return read_integer_type<std::uint16_t>(addr);
    }

    std::uint32_t read_dword(const std::uint32_t addr) {
        return read_integer_type<std::uint32_t>(addr);
    }

    std::uint64_t read_qword(const std::uint32_t addr) {
        return read_integer_type<std::uint32_t>(addr);
    }

    std::int32_t write_byte(const std::uint32_t addr, const std::uint8_t data) {
        return write_integer_type<std::uint8_t>(addr, data);
    }

    std::int32_t write_word(const std::uint32_t addr, const std::uint16_t data) {
        return write_integer_type<std::uint16_t>(addr, data);
    }

    std::int32_t write_dword(const std::uint32_t addr, const std::uint32_t data) {
        return write_integer_type<std::uint32_t>(addr, data);
    }

    std::int32_t write_qword(const std::uint32_t addr, const std::uint64_t data) {
        return write_integer_type<std::uint64_t>(addr, data);
    }
}

extern "C" {
EKA2L1_EXPORT std::uint8_t eka2l1_mem_read_byte(const std::uint32_t addr) {
    return eka2l1::scripting::read_byte(addr);
}

EKA2L1_EXPORT std::uint16_t eka2l1_mem_read_word(const std::uint32_t addr) {
    return eka2l1::scripting::read_word(addr);
}

EKA2L1_EXPORT std::uint32_t eka2l1_mem_read_dword(const std::uint32_t addr) {
    return eka2l1::scripting::read_dword(addr);
}

EKA2L1_EXPORT std::uint64_t eka2l1_mem_read_qword(const std::uint32_t addr) {
    return eka2l1::scripting::read_qword(addr);
}

EKA2L1_EXPORT std::int32_t eka2l1_mem_write_byte(const std::uint32_t addr, const std::uint8_t data) {
    return eka2l1::scripting::write_byte(addr, data);
}

EKA2L1_EXPORT std::int32_t eka2l1_mem_write_word(const std::uint32_t addr, const std::uint16_t data) {
    return eka2l1::scripting::write_word(addr, data);
}

EKA2L1_EXPORT std::int32_t eka2l1_mem_write_dword(const std::uint32_t addr, const std::uint32_t data) {
    return eka2l1::scripting::write_dword(addr, data);
}

EKA2L1_EXPORT std::int32_t eka2l1_mem_write_qword(const std::uint32_t addr, const std::uint64_t data) {
    return eka2l1::scripting::write_qword(addr, data);
}
}
