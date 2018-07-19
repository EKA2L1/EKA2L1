#pragma once

#include <core/page_table.h>
#include <core/ptr.h>

#include <functional>
#include <memory>

namespace eka2l1 {
    class memory_system {
        page_table *current_page_table;

        std::vector<page> global_pages;
        std::vector<mem_ptr> global_pointers;

        std::vector<page> codeseg_pages;
        std::vector<mem_ptr> codeseg_pointers;

        int page_size;
        uint32_t generations = 0;

        uint32_t rom_addr;
        uint32_t codeseg_addr;

        void *rom_map;

    public:
        void init(uint32_t code_ram_addr);
        void shutdown() {}

        bool map_rom(uint32_t addr, const std::string &path);

        page_table *get_current_page_table() const {
            return current_page_table;
        }

        void set_current_page_table(page_table &table) {
            current_page_table = &table;
        }

        void *get_real_pointer(address addr);

        void read(address addr, void *data, size_t size);
        void write(address addr, void *data, size_t size);

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

        uint64_t get_page_size() const {
            return page_size;
        }
    };
}