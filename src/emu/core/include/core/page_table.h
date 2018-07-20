#pragma once

#include <common/types.h>
#include <common/resource.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

enum : uint32_t {
    null_trap = 0x00000000,
    local_data = 0x00400000,
    dll_static_data = 0x38000000,
    shared_data = 0x40000000,
    ram_code_addr = 0x70000000,
    rom = 0x80000000,
    global_data = 0x90000000,
    ram_drive = 0xA0000000,
    mmu = 0xC0000000,
    io_mapping = 0xC3000000,
    page_tables = 0xC4000000,
    umem = 0xC8000000,
    kernel_mapping = 0xC9200000,
    rom_eka1 = 0x50000000,
    ram_code_addr_eka1 = 0xE0000000,
    ram_code_addr_eka1_end = 0xF0000000
};

namespace eka2l1 {
    using gen = size_t;

    enum class page_status {
        free,
        reserved,
        committed
    };

    struct page {
        gen generation;
        page_status sts;
        prot page_protection;
    };

    using mem_ptr = std::shared_ptr<uint8_t[]>;

    struct page_table {
        std::vector<mem_ptr> pointers;
        std::vector<page> pages;

        int page_size;

        page_table(int page_size);

        void read(uint32_t addr, void *dest, int size);
        void write(uint32_t addr, void *src, int size);

        template <typename T>
        T read(uint32_t addr) {
            T temp;
            read(addr, &temp, sizeof(T));

            return temp;
        }

        template <typename T>
        void write(uint32_t addr, T data) {
            write(addr, &data, sizeof(T));
        }

        void *get_ptr(uint32_t addr);
    };
}