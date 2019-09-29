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

#include <cstdint>
#include <memory>

#include <epoc/mem.h>

namespace eka2l1 {
    class memory_system;

    namespace kernel {
        class process;
    }

    namespace common {
        class chunkyseri;
    }

    // Symbian is 32 bit
    using address = uint32_t;

    void *get_raw_pointer(kernel::process *pr, address addr);

    template <typename T>
    class ptr {
        address mem_address;

    public:
        ptr()
            : mem_address(0) {}

        ptr(const uint32_t addr)
            : mem_address(addr) {}

        address ptr_address() const {
            return mem_address;
        }

        T *get(kernel::process *pr) {
            return reinterpret_cast<T *>(get_raw_pointer(pr, mem_address));
        }

        T *get(memory_system *mem, const mem::asid optional_asid = -1) const {
            return reinterpret_cast<T *>(mem->get_real_pointer(mem_address, optional_asid));
        }

        void reset() {
            mem_address = 0;
        }

        void do_state(common::chunkyseri &seri) {
            seri.absorb(mem_address);
        }

        explicit operator bool() const {
            return mem_address != 0;
        }

        const bool operator == (const eka2l1::ptr<T> rhs) const {
            return mem_address == rhs.mem_address;
        }

        template <typename U>
        ptr<U> cast() const {
            return ptr<U>(mem_address);
        }

        ptr operator+(const ptr &rhs) {
            return ptr(mem_address + rhs.mem_address);
        }

        void operator+=(const ptr &rhs) {
            mem_address += rhs.mem_address;
        }

        void operator+=(const address rhs) {
            mem_address += rhs;
        }
    };
}
