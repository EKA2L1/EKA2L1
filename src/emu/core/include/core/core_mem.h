#pragma once

#include <common/types.h>
#include <functional>
#include <memory>

namespace eka2l1 {
    template <class T>
    class ptr;

    namespace loader {
        class eka2img;
    }

    enum {
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

    class memory {
        using gen = size_t;
        using mem = std::unique_ptr<uint8_t[], std::function<void(uint8_t *)>>;
        using allocated = std::vector<gen>;

        mem memory;
        uint64_t page_size;

        gen generations;
        allocated allocated_pages;
        
    protected:
        // Alloc in a specific range
        address alloc_range(address beg, address end, size_t size);
        void alloc_inner(address addr, size_t pg_count, allocated::iterator blck);

    public:
        void init();
        void shutdown();

        // Used for User to alloc freely in local data
        address alloc(size_t size);
        void free(address addr);

        // Alloc for dynamic code execution
        address alloc_ime(size_t size);

        template <typename T>
        T *get_addr(address addr) {
            if (addr == 0) {
                return nullptr;
            }

            return reinterpret_cast<T *>(&memory[addr]);
        }

        // Load the ROM into virtual memory, using map
        bool load_rom(const std::string& rom_path);

        void* get_mem_start() {
            return memory.get();
        }

        // Map an Symbian-address
        ptr<void> map(address addr, size_t size, prot cprot);
        int change_prot(address addr, size_t size, prot nprot);
        int unmap(ptr<void> addr, size_t length);
    };
}
