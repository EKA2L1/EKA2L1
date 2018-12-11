#pragma once

#include <common/types.h>
#include <common/resource.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include <mutex>

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
    ram_code_addr_eka1_end = 0xF0000000,
    shared_data_eka1 = 0x10000000,
    shared_data_end_eka1 = 0x30000000,
    shared_data_section_size_eka2 = 0x30000000,
    shared_data_section_size_eka1 = 0x20000000,
    shared_data_section_size_max = shared_data_section_size_eka2,
    code_seg_section_size = 0x10000000,
    page_size = 0x1000,
    page_bits = 12,
    page_table_number_entries = 1 << (32 - page_bits),
    shared_data_section_max_number_entries = shared_data_section_size_max / page_size,
    code_seg_section_number_entries = code_seg_section_size / page_size
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

    struct page_table {
        std::array<std::uint8_t*, page_table_number_entries> pointers;
        std::array<page, page_table_number_entries> pages;

        std::array<page, page_table_number_entries> &get_pages();
        std::array<std::uint8_t*, page_table_number_entries> &get_pointers();

        std::mutex mut;

        int page_size;

        page_table(int page_size);

        void read(uint32_t addr, void *dest, int size);
        void write(uint32_t addr, void *src, int size);

        template <typename T>
        T read(vaddress addr) {
            T temp;
            read(addr, &temp, sizeof(T));

            return temp;
        }

        template <typename T>
        void write(vaddress addr, T data) {
            write(addr, &data, sizeof(T));
        }

        void *get_ptr(vaddress addr);
    };
}