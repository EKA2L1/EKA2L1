#pragma once

#include <kernel/chunk.h>

#include <memory>
#include <vector>

namespace eka2l1 {
    using chunk_ptr = std::shared_ptr<kernel::chunk>;

    struct heap_block {
        uint32_t offset;
        uint32_t size;
        eka2l1::ptr<void> block_ptr;

        bool free = true;
    };

    /*! \brief Use for HLE services */
    class fast_heap {
        std::vector<heap_block> *heap_blocks;
        chunk_ptr chunk;

        int align;
        int cell_count;

        uint32_t total_alloc_size = 0;

    public:
        void new_heap(chunk_ptr heap_chunk, int align = -1);
        eka2l1::ptr<void> allocate(size_t size);
        void free(eka2l1::ptr<void> ptr);

        void delete_heap(fast_heap &heap);
    };
}