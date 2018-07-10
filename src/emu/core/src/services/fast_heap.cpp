#include <services/fast_heap.h>
#include <algorithm>

namespace eka2l1 {
    void fast_heap::new_heap(chunk_ptr heap_chunk, int dealign) {
        heap_blocks = new std::vector<heap_block>();
        heap_chunk->adjust(0x1000);

        align = (dealign <= -1) ? 4 : align;

        heap_block blck;
        blck.block_ptr = heap_chunk->base().ptr_address() + sizeof(fast_heap);
        blck.free = true;
        blck.offset = sizeof(fast_heap);
        blck.size = heap_chunk->get_max_size() - sizeof(fast_heap);

        heap_blocks->push_back(blck);
    }

    eka2l1::ptr<void> fast_heap::allocate(size_t size) {
        std::vector<heap_block> &blocks = *heap_blocks;

        // If the committed size is not enough, expand the chunk
        if (total_alloc_size + size > chunk->get_size()) {
            chunk->adjust(chunk->get_size() * 2);
        }

        for (uint32_t i = 0; i < blocks.size(); i++) {
            if (blocks[i].free) {
                uint32_t block_size = blocks[i].size;

                if (blocks[i].offset % align != 0) {
                    block_size -= align - blocks[i].offset % align;
                }

                if (block_size >= size) {
                    blocks[i].size = block_size;

                    if (blocks[i].offset % align == 0) {
                        blocks[i].offset += align - blocks[i].offset % align;
                    }

                    blocks[i].block_ptr = ptr<void>(chunk->base().ptr_address() + blocks[i].offset);

                    if (blocks[i].size == size) {
                        blocks[i].free = false;

                        cell_count += 1;
                        total_alloc_size += size;

                        return blocks[i].block_ptr;
                    }

                    heap_block next_block;
                    next_block.free = true;
                    next_block.offset = blocks[i].offset + size;
                    next_block.size = blocks[i].size - size;
                    next_block.block_ptr = chunk->base().ptr_address() + next_block.offset;

                    blocks.push_back(std::move(next_block));

                    blocks[i].size = size;
                    blocks[i].free = false;

                    total_alloc_size += size;
                    cell_count += 1;

                    return blocks[i].block_ptr;
                }
            }
        }
    }

    void fast_heap::free(eka2l1::ptr<void> ptr) {
        std::vector<heap_block> &blocks = *heap_blocks;

        auto &res = std::find_if(blocks.begin(), blocks.end(),
            [&](heap_block blck) { return blck.block_ptr.ptr_address() == ptr.ptr_address(); });

        if (res == blocks.end()) {
            return;
        }

        auto idx = res - blocks.begin();

        blocks[idx].free = true;

        total_alloc_size -= res->size;
        cell_count -= 1;
    }

    void fast_heap::delete_heap(fast_heap &heap) {
        delete heap_blocks;
    }
}