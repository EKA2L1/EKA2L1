#include <core/services/fast_heap.h>
#include <core/core_mem.h>

#include <algorithm>

namespace eka2l1 {
    fast_heap::fast_heap(memory_system *mem, chunk_ptr heap_chunk, int dealign) {
        chunk = heap_chunk;
        
        heap_chunk->adjust(0x1000);

        int real_align = (dealign <= -1) ? 4 : dealign;

        heap_block blck;
        blck.free = true;
        blck.offset = sizeof(epoc::heap);
        blck.size = heap_chunk->get_max_size() - sizeof(epoc::heap);

        if (blck.offset % real_align != 0) {
            blck.offset += real_align - blck.offset % real_align;
            blck.size -= real_align - blck.offset % real_align;
        }

        blck.block_ptr = heap_chunk->base().ptr_address() + blck.offset;

        heap_blocks.push_back(blck);

        heap = heap_chunk->base().cast<epoc::heap>().get(mem);
        
        // Setup LLE heap
        heap->access_count = 0;
        heap->align = real_align;
        heap->handles = nullptr;//heap_chunk;
        heap->cell_count = 0;
        heap->total_alloc_size = 0;
        heap->handle_count = 1;
        heap->base = heap_chunk->base();
        heap->top = heap_chunk->base() + heap_chunk->get_top();
        heap->vtable = 0x3D3C3E3F; /* This is assumes that the vtable is never used from HLE client. */
    }

    eka2l1::ptr<void> fast_heap::allocate(size_t size) {
        std::vector<heap_block> &blocks = heap_blocks;

        // If the committed size is not enough, expand the chunk
        if (heap->total_alloc_size + size > chunk->get_size()) {
            chunk->adjust(chunk->get_size() * 2);
        }

        for (uint32_t i = 0; i < blocks.size(); i++) {
            if (blocks[i].free) {
                uint32_t block_size = blocks[i].size;

                if (blocks[i].offset % heap->align != 0) {
                    block_size -= heap->align - blocks[i].offset % heap->align;
                }

                if (block_size >= size) {
                    blocks[i].size = block_size;

                    if (blocks[i].offset % heap->align == 0) {
                        blocks[i].offset += heap->align - blocks[i].offset % heap->align;
                    }

                    blocks[i].block_ptr = ptr<void>(chunk->base().ptr_address() + blocks[i].offset);

                    if (blocks[i].size == size) {
                        blocks[i].free = false;

                        heap->cell_count += 1;
                        heap->total_alloc_size += size;

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

                    heap->cell_count += 1;
                    heap->total_alloc_size += size;

                    return blocks[i].block_ptr;
                }
            }
        }
    }

    bool fast_heap::free(eka2l1::ptr<void> ptr) {
        std::vector<heap_block> &blocks = heap_blocks;

        auto &res = std::find_if(blocks.begin(), blocks.end(),
            [&](heap_block blck) { return blck.block_ptr.ptr_address() == ptr.ptr_address(); });

        if (res == blocks.end()) {
            return false;
        }

        auto idx = res - blocks.begin();

        blocks[idx].free = true;

        heap->total_alloc_size -= res->size;
        heap->cell_count -= 1; 

        return true;
    }
}