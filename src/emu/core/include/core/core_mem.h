#pragma one

#include <common/types.h>

namespace eka2l1 {
    // The core memory
    namespace core_mem {
        void init();

        address alloc(size_t size);
        void free(address addr);
    }
}
