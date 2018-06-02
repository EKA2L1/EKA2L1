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

#include <common/types.h>
#include <functional>
#include <memory>

namespace eka2l1 {
    template <class T>
    class ptr;

    namespace loader {
        struct eka2img;
    }

    enum: uint32_t {
        NULL_TRAP = 0x00000000,
        LOCAL_DATA = 0x00400000,
        DLL_STATIC_DATA = 0x38000000,
        SHARED_DATA = 0x40000000,
        RAM_CODE_ADDR = 0x70000000,
        ROM = 0x80000000,
        GLOBAL_DATA = 0x90000000,
        RAM_DRIVE = 0xA0000000,
        MMU = 0xC0000000,
        IOMAPPING = 0xC3000000,
        PAGETABS = 0xC4000000,
        UMEM = 0xC8000000,
        KERNELMAPPING = 0xC9200000
    };

    using gen = size_t;
    using mem = std::unique_ptr<uint8_t[], std::function<void(uint8_t *)>>;

    enum class page_status {
        free,
        reserved,
        committed
    };

    struct page {
        gen generation;
        page_status sts;
        prot protection;
    };

    using pages = std::vector<page>;

    class memory_system {
        mem memory;
        uint64_t page_size;

        gen generations;
        pages mem_pages;

    public:
        void init();
        void shutdown();

        template <typename T>
        T *get_addr(address addr) {
            if (addr == 0) {
                return nullptr;
            }

            return reinterpret_cast<T *>(&memory[addr]);
        }

        // Load the ROM into virtual memory, using map
        bool load_rom(const std::string &rom_path);

        void *get_mem_start() const {
            return memory.get();
        }

        // Create a new chunk with specified address. Return base of chunk
        ptr<void> chunk(address addr, size_t bottom, size_t top, size_t max_grow, prot cprot);
        ptr<void> chunk_range(size_t beg_addr, size_t end_addr, size_t bottom, size_t top, size_t max_grow, prot cprot);

        // Change the prot of pages
        int change_prot(ptr<void> addr, size_t size, prot nprot);

        // Mark a chunk at addr as unusable
        int unchunk(ptr<void> addr, size_t length);

        // Commit to page
        int commit(ptr<void> addr, size_t size);

        // Decommit
        int decommit(ptr<void> addr, size_t size);
    };
}

