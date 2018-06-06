/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <epoc9/allocator.h>
#include <epoc9/base.h>
#include <epoc9/err.h>

BRIDGE_FUNC(TInt, RAllocatorOpen, eka2l1::ptr<RAllocator> aAllocator) {
    RAllocator *allocator = aAllocator.get(sys->get_memory_system());

    if (!allocator) {
        return KErrGeneral;
    }

    ++allocator->iAccessCount;

    return KErrNone;
}


BRIDGE_FUNC(void, RAllocatorClose, eka2l1::ptr<RAllocator> aAllocator) {
    RAllocator *allocator = aAllocator.get(sys->get_memory_system());

    if (!allocator) {
        LOG_WARN("**this** is nullptr");
        return;
    }

    if (--allocator->iAccessCount <= 0) {
        LOG_WARN("Allocator closes too many times, access count is down to zero");
    }

    if (allocator->iAccessCount == 1) {
        RAllocatorDoClose(sys, aAllocator);
    }
}

BRIDGE_FUNC(void, RAllocatorDoClose, eka2l1::ptr<RAllocator> aAllocator) {
    RAllocator *allocator = aAllocator.get(sys->get_memory_system());

    if (!allocator) {
        LOG_WARN("**this** is nullptr");
        return;
    }

    TInt* handles = allocator->iHandles.get(sys->get_memory_system());

    for (uint32_t i = 0; i < allocator->iHandleCount; i++) {
        RHandleBase handleBase;
        RHandleBaseSetHandleNoHLEPointer(sys, &handleBase, handles[i]);
        RHandleBaseCloseNoHLEPointer(sys, &handleBase);
    }
}

using namespace eka2l1;

BRIDGE_FUNC(eka2l1::ptr<TAny>, RHeapAlloc, eka2l1::ptr<RHeap> aHeap, TInt aSize) {
    kernel_system *kern = sys->get_kernel_system();

    RHeapAdvance *heap_adv = reinterpret_cast<RHeapAdvance*>(aHeap.get(sys->get_memory_system()));
    std::vector<SBlock> &blocks = *reinterpret_cast<std::vector<SBlock>*>(heap_adv->iBlocks);
    chunk_ptr chnk = std::reinterpret_pointer_cast<kernel::chunk>(kern->get_kernel_obj(heap_adv->iChunkHandle));

    if (chnk == nullptr) {
        return ptr<TAny>(nullptr);
    }

    int align = heap_adv->iAlign;

    // If the committed size is not enough, expand the chunk
    if (heap_adv->iTotalAllocSize + aSize > chnk->get_size()) {
        chnk->adjust(chnk->get_size() * 2);
    }

    for (uint32_t i = 0; i < blocks.size(); i++) {
        if (blocks[i].free) {
            uint32_t block_size = blocks[i].size;

            if (blocks[i].offset % heap_adv->iAlign != 0) {
                block_size -= align - blocks[i].offset % align;
            }

            if (block_size >= aSize) {
                blocks[i].size = block_size;

                if (blocks[i].offset % align == 0) {
                    blocks[i].offset += align - blocks[i].offset % align;
                }

                blocks[i].block_ptr = ptr<void>(heap_adv->iBase.ptr_address() + blocks[i].offset);

                if (blocks[i].size == aSize) {
                    blocks[i].free = false;

                    heap_adv->iCellCount += 1;
                    heap_adv->iTotalAllocSize += aSize;

                    return blocks[i].block_ptr;
                }

                SBlock next_block;
                next_block.free = true;
                next_block.offset = blocks[i].offset + aSize;
                next_block.size = blocks[i].size - aSize;
                blocks.push_back(next_block);

                blocks[i].size = aSize;
                blocks[i].free = false;

                heap_adv->iTotalAllocSize += aSize;
                heap_adv->iCellCount += 1;

                return blocks[i].block_ptr;
            }
        }
    }
}

BRIDGE_FUNC(void, RHeapFree, eka2l1::ptr<RHeap> aHeap, ptr<TAny> aPtr) {
    kernel_system *kern = sys->get_kernel_system();

    RHeapAdvance *heap_adv = reinterpret_cast<RHeapAdvance*>(aHeap.get(sys->get_memory_system()));
    std::vector<SBlock> &blocks = *reinterpret_cast<std::vector<SBlock>*>(heap_adv->iBlocks);

    auto& res = std::find_if(blocks.begin(), blocks.end(),
        [aPtr](auto blck) { return blck.block_ptr == aPtr; });

    if (res == blocks.end()) {
        LOG_WARN("Free a undenifed cell.");
        return;
    }

    res->free = true;
    
    heap_adv->iTotalAllocSize -= res->size;
    heap_adv->iCellCount -= 1;
}

BRIDGE_FUNC(void, RAllocatorDbgMarkStart, eka2l1::ptr<RAllocator> aAllocator) {

}

const eka2l1::hle::func_map allocator_register_funcs = {
    BRIDGE_REGISTER(3787177057, RAllocatorOpen),
    BRIDGE_REGISTER(1629345542, RAllocatorClose),
    BRIDGE_REGISTER(381559899, RAllocatorDoClose),
    BRIDGE_REGISTER(2261851716, RAllocatorDbgMarkStart),
    BRIDGE_REGISTER(2899467538, RHeapFree),
    BRIDGE_REGISTER(179745759, RHeapAlloc)
};
