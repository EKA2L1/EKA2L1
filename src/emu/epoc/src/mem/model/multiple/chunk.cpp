#include <epoc/mem/model/multiple/chunk.h>

namespace eka2l1::mem {
    std::size_t multiple_mem_model_chunk::commit(const vm_address offset, const std::size_t size) {
        return 0;
    }

    void multiple_mem_model_chunk::decommit(const vm_address offset, const std::size_t size) {
        return;
    }
}
