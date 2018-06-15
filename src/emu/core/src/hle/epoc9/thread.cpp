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

    bool committed = chunk_ptr->commit(aOffset, aMinLength);

    if (!committed) {
        bool adjted = chunk_ptr->adjust(aMinLength);

        if (!adjted) {
            return ptr<RHeap>(nullptr);
        }
    }

    hle::lib_manager *mngr = sys->get_lib_manager();

    RHeapAdvance heap = NewHeap(sys, mngr, chunk_ptr, aOffset, aAlign);

    // For now, just take the default heap, idk
    memcpy(chunk_ptr->base().get(mem) + aOffset, &heap, sizeof(RHeapAdvance));
    address ret =  chunk_ptr->base().ptr_address() + aOffset;

    // Set the public User:: allocator to this
    if (aMode & KHeapReplace)
        current_local_data(sys).heap = ret;

    return ret;
}

BRIDGE_FUNC(TInt, RThreadCreate, eka2l1::ptr<RThread> aThread, eka2l1::ptr<TDesC> aName, eka2l1::ptr<TThreadFunction> aFunction,
    TInt aStackSize, TInt aHeapMinSize, TInt aHeapMaxSize, eka2l1::ptr<TAny> aPtr, TOwnerType aType) {
    memory_system *mem = sys->get_memory_system();
    kernel_system *kern = sys->get_kernel_system();
    RThread *thr_wrap = aThread.get(mem);

    kernel::owner_type owner = aType == EOwnerProcess ? kernel::owner_type::process : kernel::owner_type::thread;
    std::string thread_name = common::ucs2_to_utf8(aName.get(mem)->StdString(sys));

    thread_ptr hle_thread = kern->add_thread(owner,
        kern->get_id_base_owner(owner), kernel::access_type::local_access,
        thread_name, aFunction.ptr_address(), aStackSize, aHeapMinSize, aHeapMaxSize, aPtr.get(mem));

    thr_wrap->iHandle = hle_thread->unique_id();

    chunk_ptr thread_heap_chunk = kern->create_chunk("HeapThread" + std::to_string(thr_wrap->iHandle),
        0, aHeapMinSize, aHeapMaxSize, prot::read_write_exec, kernel::chunk_type::normal, kernel::chunk_access::local,
        kernel::chunk_attrib::none, kernel::owner_type::thread);

    RHeapAdvance thread_heap = NewHeap(sys, sys->get_lib_manager(), thread_heap_chunk, 0, 4);

    // For now, just take the default heap, idk
    memcpy(thread_heap_chunk->base().get(mem), &thread_heap, sizeof(RHeapAdvance));
    address ret = thread_heap_chunk->base().ptr_address();

    // Swapping the heap
    current_local_data(sys).heap = ret;

    return KErrNone;
}

BRIDGE_FUNC(TInt, RThreadCreateWithAllocator, eka2l1::ptr<RThread> aThread, eka2l1::ptr<TDesC> aName, eka2l1::ptr<TThreadFunction> aFunction,
    TInt aStackSize, eka2l1::ptr<RAllocator> aAllocator, eka2l1::ptr<TAny> aPtr, TOwnerType aType) {
    memory_system *mem = sys->get_memory_system();
    kernel_system *kern = sys->get_kernel_system();
    RThread *thr_wrap = aThread.get(mem);

    kernel::owner_type owner = aType == EOwnerProcess ? kernel::owner_type::process : kernel::owner_type::thread;
    std::string thread_name = common::ucs2_to_utf8(aName.get(mem)->StdString(sys));

    thread_ptr hle_thread = kern->add_thread(owner,
        kern->get_id_base_owner(owner), kernel::access_type::local_access,
        thread_name, aFunction.ptr_address(), aStackSize, 0, 0, aPtr.get(mem));

    thr_wrap->iHandle = hle_thread->unique_id();

    // Swapping the heap
    current_local_data(sys).heap = aAllocator.ptr_address();

    return KErrNone;
}

BRIDGE_FUNC(TInt, RThreadOpen, eka2l1::ptr<RThread> aThread, kernel::uid aId, TOwnerType aOwner) {
    kernel_system *kern = sys->get_kernel_system();
    kernel_obj_ptr hle_obj = kern->get_kernel_obj(aId); 

    if (!hle_obj) {
        return KErrNotFound;
    }

    thread_ptr maybe_thread = std::dynamic_pointer_cast<kernel::thread>(hle_obj);

    if (!maybe_thread) {
        return KErrNotFound;
    }

    hle_obj->set_owner_type(aOwner == EOwnerProcess ? kernel::owner_type::process : kernel::owner_type::thread);
    aThread.get(sys->get_memory_system())->iHandle = aId;

    return KErrNone;
}

BRIDGE_FUNC(TInt, RThreadOpenByName, eka2l1::ptr<RThread> aThread, eka2l1::ptr<TDesC> aName, TOwnerType aOwner) {
    kernel_system *kern = sys->get_kernel_system();
    TDesC *des = aName.get(sys->get_memory_system());
    thread_ptr thr = kern->get_thread_by_name(common::ucs2_to_utf8(des->StdString(sys)));

    if (!thr) {
        return KErrNotFound;
    }

    thr->set_owner_type(aOwner == EOwnerProcess ? kernel::owner_type::process : kernel::owner_type::thread);
    aThread.get(sys->get_memory_system())->iHandle = thr->unique_id();

    return KErrNone;
}

// Funny name
BRIDGE_FUNC(TInt, RThreadRenameMe, eka2l1::ptr<RThread> aThread, eka2l1::ptr<TDes> aNewName) {
    kernel_system *kern = sys->get_kernel_system();
    TDesC *des = aNewName.get(sys->get_memory_system());
    RThread *lle_thread = aThread.get(sys->get_memory_system());

    kernel_obj_ptr hle_obj = kern->get_kernel_obj(lle_thread->iHandle);

    if (!hle_obj) {
        return KErrNotFound;
    }

    hle_obj->rename(common::ucs2_to_utf8(des->StdString(sys)));

    return KErrNone;
}

BRIDGE_FUNC(TInt, RThreadKill, eka2l1::ptr<RThread> aThread, TInt aReason) {
    kernel_system *kern = sys->get_kernel_system();
    RThread *lle_thread = aThread.get(sys->get_memory_system());

    kernel_obj_ptr hle_obj = kern->get_kernel_obj(lle_thread->iHandle);

    if (!hle_obj) {
        return KErrNotFound;
    }

    thread_ptr hle_thread = std::reinterpret_pointer_cast<kernel::thread>(hle_obj);
    hle_thread->stop();

    LOG_INFO("Thread is either killed or terminated with no mercy, sadly, with code: {}", aReason);

    return KErrNone;
}

BRIDGE_FUNC(TInt, RThreadTerminate, eka2l1::ptr<RThread> aThread, TInt aReason) {
    return RThreadKill(sys, aThread, aReason);
}

const eka2l1::hle::func_map thread_register_funcs = {
    BRIDGE_REGISTER(479498917, UserHeapCreateThreadHeap),
    BRIDGE_REGISTER(1478769233, UserHeapChunkHeap),
    BRIDGE_REGISTER(1342626600, RThreadCreateWithAllocator),
    BRIDGE_REGISTER(2297872534, RThreadCreate),
    BRIDGE_REGISTER(1042284812, RThreadOpen),
    BRIDGE_REGISTER(3698492392, RThreadOpenByName),
    BRIDGE_REGISTER(3496173659, RThreadKill),
    BRIDGE_REGISTER(1807821789, RThreadTerminate)
};