#pragma one

#include <common/types.h>
#include <memory>
#include <functional>

namespace eka2l1 {
    namespace loader {
        class eka2img;
    }

    enum {
        NULL_TRAP     = 0x00000000,
        LOCAL_DATA    = 0x00400000,
        DLL_STATIC_DATA = 0x38000000,
        SHARED_DATA = 0x40000000,
        RAM_CODE_ADDR = 0x70000000,
        ROM           = 0x80000000,
        GLOBAL_DATA   = 0x90000000,
        RAM_DRIVE     = 0xA0000000,
        MMU           = 0xC0000000,
        IOMAPPING     = 0xC3000000,
        PAGETABS      = 0xC4000000,
        UMEM          = 0xC8000000,
        KERNELMAPPING = 0xC9200000
    };

    template <class T>
    class ptr;

    // The core memory
    namespace core_mem {
        using mem = std::unique_ptr<uint8_t[], std::function<void(uint8_t*)>>;

        extern mem memory;
        extern uint64_t page_size;

        void init();
        void shutdown();

        address alloc(size_t size);
        void free(address addr);

        template <typename T>
        T* get_addr(address addr) {
            if (addr == 0) {
                return nullptr;
            }

            return reinterpret_cast<T*>(&memory[addr]);
        }

        // Map an Symbian-address
        ptr<void> map(address addr, size_t size, prot cprot);
        int      change_prot(address addr, size_t size, prot nprot);
        int  unmap(ptr<void> addr, size_t length);
    }
}
