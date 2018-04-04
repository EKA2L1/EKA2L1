#pragma one

#include <common/types.h>

namespace eka2l1 {
    // The core memory
    namespace core_mem {
        using mem = std::unique_ptr<uint8_t[], std::function<void(uint8_t*)>>;

        extern mem memory;

        void init();
        void shutdown();

        address alloc(size_t size);
        void free(address addr);

        template <typename T>
        T* get(address addr) {
            if (addr == 0) {
                return nullptr;
            }

            return reinterpret_cast<T*>(&memory[addr]);
        }
    }
}
