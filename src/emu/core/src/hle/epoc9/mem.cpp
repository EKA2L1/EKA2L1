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

#include <epoc9/err.h>
#include <epoc9/mem.h>

#include <common/cvt.h>

BRIDGE_FUNC(TInt, RChunkAdjustDoubleEnded, eka2l1::ptr<RChunk> aThis, TInt aNewBottom, TInt aNewTop) {
    return KErrNone;
}

BRIDGE_FUNC(TInt, RChunkAllocate, eka2l1::ptr<RChunk> aThis, TInt aSize) {
    return KErrNone;
}

BRIDGE_FUNC(eka2l1::ptr<TUint8>, RChunkBase, eka2l1::ptr<RChunk> aThis) {
    RChunk *chunk = aThis.get(sys->get_memory_system());

    eka2l1::kernel_system *kern = sys->get_kernel_system();
    eka2l1::kernel_obj_ptr obj_ptr = kern->get_kernel_obj(chunk->iHandle);

    if (!obj_ptr) {
        LOG_WARN("Chunk kernel object unavailable");
        return -1;
    }

    eka2l1::chunk_ptr chunk_ptr = std::reinterpret_pointer_cast<eka2l1::kernel::chunk>(obj_ptr);
    return chunk_ptr->base();
}

BRIDGE_FUNC(TInt, RChunkBottom, eka2l1::ptr<RChunk> aThis) {
    RChunk *chunk = aThis.get(sys->get_memory_system());

    eka2l1::kernel_system *kern = sys->get_kernel_system();
    eka2l1::kernel_obj_ptr obj_ptr = kern->get_kernel_obj(chunk->iHandle);

    if (!obj_ptr) {
        LOG_WARN("Chunk kernel object unavailable");
        return -1;
    }

    eka2l1::chunk_ptr chunk_ptr = std::reinterpret_pointer_cast<eka2l1::kernel::chunk>(obj_ptr);
    return static_cast<TInt>(chunk_ptr->get_bottom());
}

BRIDGE_FUNC(TInt, RChunkTop, eka2l1::ptr<RChunk> aThis) {
    RChunk *chunk = aThis.get(sys->get_memory_system());

    eka2l1::kernel_system *kern = sys->get_kernel_system();
    eka2l1::kernel_obj_ptr obj_ptr = kern->get_kernel_obj(chunk->iHandle);

    if (!obj_ptr) {
        LOG_WARN("Chunk kernel object unavailable");
        return -1;
    }

    eka2l1::chunk_ptr chunk_ptr = std::reinterpret_pointer_cast<eka2l1::kernel::chunk>(obj_ptr);
    return static_cast<TInt>(chunk_ptr->get_top());
}

BRIDGE_FUNC(TInt, RChunkMaxSize, eka2l1::ptr<RChunk> aThis) {
    RChunk *chunk = aThis.get(sys->get_memory_system());

    eka2l1::kernel_system *kern = sys->get_kernel_system();
    eka2l1::kernel_obj_ptr obj_ptr = kern->get_kernel_obj(chunk->iHandle);

    if (!obj_ptr) {
        LOG_WARN("Chunk kernel object unavailable");
        return -1;
    }

    eka2l1::chunk_ptr chunk_ptr = std::reinterpret_pointer_cast<eka2l1::kernel::chunk>(obj_ptr);
    return static_cast<TInt>(chunk_ptr->get_max_size());
}

BRIDGE_FUNC(TInt, RChunkCommit, eka2l1::ptr<RChunk> aThis, TInt aOffset, TInt aSize) {
    RChunk *chunk = aThis.get(sys->get_memory_system());

    eka2l1::kernel_system *kern = sys->get_kernel_system();
    eka2l1::kernel_obj_ptr obj_ptr = kern->get_kernel_obj(chunk->iHandle);

    if (!obj_ptr) {
        LOG_WARN("Chunk kernel object unavailable");
        return KErrBadHandle;
    }

    eka2l1::chunk_ptr chunk_ptr = std::reinterpret_pointer_cast<eka2l1::kernel::chunk>(obj_ptr);

    if (chunk_ptr->get_chunk_type() != kernel::chunk_type::disconnected) {
        return KErrGeneral;
    }

    chunk_ptr->commit(aOffset, aSize);

    return KErrNone;
}

BRIDGE_FUNC(TInt, RChunkDecommit, eka2l1::ptr<RChunk> aThis, TInt aOffset, TInt aSize) {
    RChunk *chunk = aThis.get(sys->get_memory_system());

    eka2l1::kernel_system *kern = sys->get_kernel_system();
    eka2l1::kernel_obj_ptr obj_ptr = kern->get_kernel_obj(chunk->iHandle);

    if (!obj_ptr) {
        LOG_WARN("Chunk kernel object unavailable");
        return KErrBadHandle;
    }

    eka2l1::chunk_ptr chunk_ptr = std::reinterpret_pointer_cast<eka2l1::kernel::chunk>(obj_ptr);

    if (chunk_ptr->get_chunk_type() != kernel::chunk_type::disconnected) {
        return KErrGeneral;
    }

    chunk_ptr->decommit(aOffset, aSize);

    return KErrNone;
}

BRIDGE_FUNC(TInt, RChunkAdjust, eka2l1::ptr<RChunk> aThis, TInt aSize) {
    RChunk *chunk = aThis.get(sys->get_memory_system());

    eka2l1::kernel_system *kern = sys->get_kernel_system();
    eka2l1::kernel_obj_ptr obj_ptr = kern->get_kernel_obj(chunk->iHandle);

    if (!obj_ptr) {
        LOG_WARN("Chunk kernel object unavailable");
        return KErrBadHandle;
    }

    eka2l1::chunk_ptr chunk_ptr = std::reinterpret_pointer_cast<eka2l1::kernel::chunk>(obj_ptr);

    bool res = chunk_ptr->adjust(aSize);

    if (!res) {
        return KErrGeneral;
    }

    return KErrNone;
}

using namespace eka2l1::kernel;

chunk_type FetchChunkType(TInt iType) {
    if (iType & TChunkCreateAtt::ENormal) {
        return kernel::chunk_type::normal;
    } else {
        if (iType & TChunkCreateAtt::EDoubleEnded) {
            return kernel::chunk_type::double_ended;
        }
    }

    return kernel::chunk_type::disconnected;
}

prot FetchProt(TInt iType) {
    if (iType & TChunkCreateAtt::ECode) {
        return prot::read_write_exec;
    }

    return prot::read_write;
}

// Create a Chunk. This function eliminate HLE pointer
TInt RChunkCreateHLEPointerEliminate(eka2l1::system *sys, RChunk *chunk, TChunkCreateInfo *create_info) {
    bool global = create_info->iGlobal;

    chunk_type ctype = FetchChunkType(create_info->iType);
    prot cprot = FetchProt(create_info->iType);

    std::string name = "";

    if (((create_info->iAttributes & TChunkCreateAtt::ELocalNamed) && !global) || global) {
        // TODO: Replace with real name from TDesC
        TDesC16 *des = create_info->iName.get(sys->get_memory_system());

        if (des) {
            TUint16 *raw_name = GetTDes16Ptr(sys, des);
            name = common::ucs2_to_utf8(std::u16string(raw_name, raw_name + des->iLength));
        }
    }

    owner_type owner = (create_info->iOwnerType == TOwnerType::EOwnerProcess) ? owner_type::process : owner_type::thread;

    eka2l1::kernel_system *kern = sys->get_kernel_system();
    eka2l1::chunk_ptr hle_chunk = kern->create_chunk(name, create_info->iInitialBottom, create_info->iInitialTop, create_info->iMaxSize,
        cprot, ctype, global ? chunk_access::global : chunk_access::local, !create_info->iName ? chunk_attrib::anonymous : chunk_attrib::none, owner);

    chunk->iHandle = hle_chunk->unique_id();

    return KErrNone;
}

BRIDGE_FUNC(TInt, RChunkCreate, eka2l1::ptr<RChunk> aThis, eka2l1::ptr<TChunkCreateInfo> aInfo) {
    RChunk *chunk = aThis.get(sys->get_memory_system());
    TChunkCreateInfo *create_info = aInfo.get(sys->get_memory_system());

    if (!create_info || !chunk) {
        return KErrGeneral;
    }

    return RChunkCreateHLEPointerEliminate(sys, chunk, create_info);
}

BRIDGE_FUNC(TInt, RChunkCreateDisconnectLocal, eka2l1::ptr<RChunk> aThis, TInt aInitBottom, TInt aInitTop, TInt aMaxSize, TOwnerType aType) {
    RChunk *chunk = aThis.get(sys->get_memory_system());

    if (!chunk) {
        return -1;
    }

    TChunkCreateInfo info;
    info.iType = TChunkCreateAtt::EDisconnected;
    info.iGlobal = false;
    info.iInitialBottom = aInitBottom;
    info.iInitialTop = aInitTop;
    info.iOwnerType = aType;

    return RChunkCreateHLEPointerEliminate(sys, chunk, &info);
}

BRIDGE_FUNC(TInt, RChunkOpenGlobal, eka2l1::ptr<RChunk> aChunk, eka2l1::ptr<TDesC> aChunkName, TInt aReadOnly, TOwnerType aType) {
    auto name = common::ucs2_to_utf8(aChunkName.get(sys->get_memory_system())->StdString(sys));

    int new_id = sys->get_kernel_system()->mirror_chunk(name, aType == EOwnerProcess ? eka2l1::kernel::owner_type::process : eka2l1::kernel::owner_type::thread);

    if (new_id == -1) {
        return KErrNotFound;
    }

    RChunk *chunk = aChunk.get(sys->get_memory_system());
    chunk->iHandle = new_id;

    return KErrNone;
}

BRIDGE_FUNC(void, MemFill, eka2l1::ptr<TAny> aTrg, TInt aLen, TChar aChar) {
    TUint8 *ptr = reinterpret_cast<TUint8 *>(aTrg.get(sys->get_memory_system()));

    if (!ptr) {
        LOG_CRITICAL("Mem Filling encounters nullptr, no Panic raised.");
        return;
    }

    // From both Symbian 6.1 and 9.4 devlib: The function assumes that the fill character
    // is a non-Unicode character.
    std::fill(ptr, ptr + aLen, static_cast<TUint8>(aChar.iChar));
}

BRIDGE_FUNC(void, MemFillZ, eka2l1::ptr<TAny> aTrg, TInt aLen) {
    MemFill(sys, aTrg, aLen, TChar{ 0 });
}

BRIDGE_FUNC(void, MemSwap, ptr<void> aLhs, ptr<void> aRhs, TInt aLen) {
    memory_system *mem = sys->get_memory_system();

    if (!aLhs || !aRhs) {
        LOG_WARN("Swapping nullptr");
        return;
    }

    uint8_t *holder = reinterpret_cast<uint8_t *>(malloc(aLen));
    memcpy(holder, aLhs.get(mem), aLen);
    memmove(aLhs.get(mem), aRhs.get(mem), aLen);
    memmove(aRhs.get(mem), holder, aLen);

    free(holder);
}

const eka2l1::hle::func_map mem_register_funcs = {
    BRIDGE_REGISTER(2758081926, RChunkCreateDisconnectLocal),
    BRIDGE_REGISTER(3201211097, RChunkTop),
    BRIDGE_REGISTER(3131320258, RChunkBase),
    BRIDGE_REGISTER(376494078, RChunkBottom),
    BRIDGE_REGISTER(762561902, RChunkCommit),
    BRIDGE_REGISTER(2317460249, RChunkDecommit),
    BRIDGE_REGISTER(3839845899, RChunkMaxSize),
    BRIDGE_REGISTER(3291428935, RChunkAdjust),
    BRIDGE_REGISTER(261677635, MemSwap),
    BRIDGE_REGISTER(1704780047, RChunkOpenGlobal)
};