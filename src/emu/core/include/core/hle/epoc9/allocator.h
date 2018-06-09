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
#pragma once

#include <epoc9/types.h>
#include <epoc9/lock.h>
#include <hle/bridge.h>

#include <ptr.h>

using namespace eka2l1::hle;

struct MAllocator {
    eka2l1::ptr<void> iVtable;
};

struct RAllocator: public MAllocator {
    TInt	iAccessCount;
    TInt	iCellCount;
    TUint32	iFlags;
    TInt	iHandleCount;
    eka2l1::ptr<TInt>	iHandles;
    TInt	iTotalAllocSize;
};

struct SCell {
    TInt   len;
    SCell* next;
};

struct RHeap : public RAllocator {
    TInt    iUnused1;
    TInt    iMaxLength;
    TInt    iUnused2;
    TInt    iUnused3;
    TInt	iChunkHandle;
    RFastLock	iLock;
    eka2l1::ptr<uint8_t> iBase;
    eka2l1::ptr<uint8_t> iTop;
    TInt iAlign;
    TInt iAllocFail;
    TInt iNestingLevel;
    TInt iNestData;
};

// Our heap customized

struct SBlock {
    TInt offset;
    TInt size;
    eka2l1::ptr<void> block_ptr;

    bool free = true;
};

struct RHeapAdvance : public RHeap {
    TUint64 iBlocks;
};

RHeapAdvance NewHeap(eka2l1::system *sys, eka2l1::hle::lib_manager *mngr, eka2l1::chunk_ptr chnk, address offset, int align);
void FreeHeap(RHeapAdvance heap);

BRIDGE_FUNC(void, RAllocatorDoClose, eka2l1::ptr<RAllocator> aAllocator);
BRIDGE_FUNC(TInt, RAllocatorOpen, eka2l1::ptr<RAllocator> aAllocator);
BRIDGE_FUNC(void, RAllocatorClose, eka2l1::ptr<RAllocator> aAllocator);
BRIDGE_FUNC(void, RAllocatorDbgMarkStart, eka2l1::ptr<RAllocator> aAllocator);

BRIDGE_FUNC(eka2l1::ptr<TAny>, RHeapAlloc, eka2l1::ptr<RHeap> aHeap, TInt aSize);
BRIDGE_FUNC(eka2l1::ptr<TAny>, RHeapAllocZ, eka2l1::ptr<RHeap> aHeap, TInt aSize);
BRIDGE_FUNC(void, RHeapFree, eka2l1::ptr<RHeap> aHeap, eka2l1::ptr<TAny> aPtr);
BRIDGE_FUNC(eka2l1::ptr<TAny>, RHeapAllocZL, eka2l1::ptr<RHeap> aHeap, TInt aSize);

extern const eka2l1::hle::func_map allocator_register_funcs;