#pragma one

#include <common/types.h>
#include <memory>
#include <functional>

namespace eka2l1 {
    enum {
        RAM_CODE_ADDR = 0x70000000
    };

    template <class T>
    class ptr;

    // The core memory
    namespace core_mem {
        using mem = std::unique_ptr<uint8_t[], std::function<void(uint8_t*)>>;

        extern mem memory;

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

        enum class prot {
            read = 0,
            write = 1,
            exec = 2,
            read_write = read | write,
            read_exec = read | exec,
            read_write_exec = read_write | exec
        };

        // Map an Symbian-address
        ptr<void> map(address addr, size_t size, prot cprot);
        int  unmap(ptr<uint8_t> addr, size_t length);
    }
}
