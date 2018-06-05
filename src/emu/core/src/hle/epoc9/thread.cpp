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

#include <common/algorithm.h>
#include <common/cvt.h>

#include <epoc9/thread.h>
#include <epoc9/mem.h>
#include <epoc9/user.h>

// Replace the Heap with old one in local storage
const TInt KHeapReplace = 1;

// Not use the provided chunk for heap, but duplicate it
const TInt KHeapChunkDuplicate = 2;

BRIDGE_FUNC(TInt, UserHeapCreateThreadHeap, ptr<SStdEpocThreadCreateInfo> aInfo, ptr<address> iHeapPtr, TInt aAlign, TBool aSingleThread) {
    memory_system *mem = sys->get_memory_system();
    SStdEpocThreadCreateInfo *info_ptr = aInfo.get(mem);
    
    TInt page_size = sys->get_memory_system()->get_page_size();
    TInt min_len = common::align(info_ptr->iHeapInitialSize, aAlign);
    TInt max_len = common::max(info_ptr->iHeapMaxSize, min_len);

    TUint16 *name_16 = info_ptr->iName.iPtr.get(mem);

    // I'm sorry.
    std::string name_str = common::ucs2_to_utf8(std::u16string(name_16, name_16 + info_ptr->iName.iLength)) + " Heap Senpai";

    chunk_ptr heap_chunk = sys->get_kernel_system()->create_chunk(name_str, 0, 0,
        max_len * 2, prot::read_write, kernel::chunk_type::disconnected, kernel::chunk_access::local,
        kernel::chunk_attrib::none, kernel::owner_type::thread);

    RChunk chunk_temp;
    chunk_temp.iHandle = heap_chunk->unique_id();

    *iHeapPtr.get(mem) = UserHeapChunkHeap(sys, chunk_temp, min_len, 0, page_size, max_len, aAlign, aSingleThread, KHeapReplace | KHeapChunkDuplicate).ptr_address();
  
    // Since we duplicated the heap, remove it
    sys->get_kernel_system()->close_chunk(heap_chunk->unique_id());

    return 0;
}

BRIDGE_FUNC(ptr<RHeap>, UserHeapChunkHeap, RChunk aChunk, TInt aMinLength, TInt aOffset, TInt aGrowBy, TInt aMaxLength, TInt aAlign, TBool aSingleThread, TUint32 aMode) {
    memory_system *mem = sys->get_memory_system();
    kernel_system *kern = sys->get_kernel_system();

    TInt page_size = mem->get_page_size();

    TInt align = 8;

    if (aMaxLength > 0) {
        if (aMaxLength < aMinLength) {
            LOG_ERROR("Chunk heap error: min is bigger than max");
        }
    }

    eka2l1::kernel_obj_ptr obj_ptr = kern->get_kernel_obj(aChunk.iHandle);
    
    if (!obj_ptr) {
        LOG_WARN("Chunk kernel object unavailable");
        return KErrBadHandle;
    }

    eka2l1::chunk_ptr chunk_ptr = std::reinterpret_pointer_cast<eka2l1::kernel::chunk>(obj_ptr);

    if (aMode & KHeapChunkDuplicate) {
        decltype(chunk_ptr) old_chunk_ptr = chunk_ptr;
        chunk_ptr = kern->create_chunk(old_chunk_ptr->name() + " Duplicated", 0, 0, aMaxLength, prot::read_write,
            old_chunk_ptr->get_chunk_type(), kernel::chunk_access::local, kernel::chunk_attrib::none, old_chunk_ptr->get_owner_type());
    }

    aMinLength = common::align(aMinLength + aOffset, page_size);
    aMaxLength = common::align(aMaxLength + aOffset, page_size);

    aMinLength -= aOffset;
    aMaxLength -= aOffset;

    // Make it in middle

    aMaxLength = common::align(aMaxLength >> 1, page_size);
    aOffset = aMaxLength;

    bool committed = chunk_ptr->commit(aOffset, aMinLength);

    if (!committed) {
        bool adjted = chunk_ptr->adjust(aMinLength);

        if (!adjted) {
            return ptr<RHeap>(nullptr);
        }
    }

    hle::lib_manager *mngr = sys->get_lib_manager();

    RHeap heap;

    heap.iVtable = ptr<void>(mngr->get_vtable_address("RHeap"));
    heap.iPageSize = page_size;
    heap.iBase = ptr<TUint8>(chunk_ptr->base().ptr_address() + aOffset);
    heap.iGrowBy = aGrowBy;
    heap.iOffset = aOffset;
    heap.iAccessCount = 0;
    heap.iTotalAllocSize = 0;
    heap.iChunkHandle = chunk_ptr->unique_id();
    
    // For now, just take the default heap, idk
    memcpy(chunk_ptr->base().get(mem) + aOffset, &heap, sizeof(RHeap));
    address ret =  chunk_ptr->base().ptr_address() + aOffset;

    if (aMode & KHeapReplace)
    // Set the public User:: allocator to this
        current_local_data(sys).heap = ret;

    return ret;
}

const eka2l1::hle::func_map thread_register_funcs = {
    BRIDGE_REGISTER(479498917, UserHeapCreateThreadHeap),
    BRIDGE_REGISTER(1478769233, UserHeapChunkHeap)
};