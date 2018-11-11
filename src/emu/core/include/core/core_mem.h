#pragma once

#include <core/page_table.h>

#include <array>
#include <functional>
#include <memory>

namespace eka2l1 {
    class system;

    template <typename T>
    class ptr;

    namespace arm {
        class jit_interface;

        using jitter = std::unique_ptr<jit_interface>;
    }

    class memory_system {
        friend class system;

        page_table *current_page_table = nullptr;
        page_table *previous_page_table = nullptr;

        std::array<page, shared_data_section_max_number_entries> global_pages;
        std::array<std::uint8_t*, shared_data_section_max_number_entries> global_pointers;

        std::array<page, code_seg_section_number_entries> codeseg_pages;
        std::array<std::uint8_t*, code_seg_section_number_entries> codeseg_pointers;

        std::size_t rom_size;

        int page_size;
        uint32_t generations = 0;

        uint32_t rom_addr;
        uint32_t codeseg_addr;

        uint32_t shared_addr;
        uint32_t shared_size;

        void *rom_map;

        arm::jit_interface *cpu;

    public:
        void init(arm::jitter &jit, uint32_t code_ram_addr,
            uint32_t shared_addr, uint32_t shared_size);

        void shutdown();

        bool map_rom(uint32_t addr, const std::string &path);

        page_table *get_current_page_table() const;
        void set_current_page_table(page_table &table);

        void *get_real_pointer(address addr);

        bool read(address addr, void *data, uint32_t size);
        bool write(address addr, void *data, uint32_t size);

        template <typename T>
        T read(address addr) {
            T data{};
            read(addr, &data, sizeof(T));

            return data;
        }

        template <typename T>
        bool write(address addr, const T &val) {
            return write(addr, &val, sizeof(T));
        }

        // Create a new chunk with specified address. Return base of chunk
        ptr<void> chunk(address addr, uint32_t bottom, uint32_t top, uint32_t max_grow, prot cprot);
        ptr<void> chunk_range(address beg_addr, address end_addr, uint32_t bottom, uint32_t top, uint32_t max_grow, prot cprot);

        // Change the prot of pages
        int change_prot(ptr<void> addr, uint32_t size, prot nprot);

        // Mark a chunk at addr as unusable
        int unchunk(ptr<void> addr, uint32_t length);

        // Commit to page
        int commit(ptr<void> addr, uint32_t size);

        // Decommit
        int decommit(ptr<void> addr, uint32_t size);

        int get_page_size() const {
            return page_size;
        }
    };
}