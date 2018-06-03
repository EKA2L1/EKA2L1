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

#include <epoc9/mem.h>
#include <epoc9/err.h>

BRIDGE_FUNC(TInt, RChunkAdjust, eka2l1::ptr<RChunk> aThis, TInt aNewSize) {
    return KErrNone;
}

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
    chunk_ptr->decommit(aOffset, aSize);

    return KErrNone;
}


using namespace eka2l1::kernel;

chunk_type FetchChunkType(TInt iType) {
    if (iType & TChunkCreateAtt::ENormal) {
        return chunk_type::normal;
    } else {
        if (iType & TChunkCreateAtt::EDoubleEnded) {
            return chunk_type::double_ended;
        }
    }

    return chunk_type::disconnected;
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

    if ((create_info->iAttributes & TChunkCreateAtt::ELocalNamed) && !global) {
        // TODO: Replace with real name from TDesC
        name = "placeholder_localnamedforce";
    }
    else {
        name = "placeholder_noforcename";
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

BRIDGE_FUNC(TInt, RChunkCreateDisconnectLocal, eka2l1::ptr<RChunk> aThis, TInt aInitBottom, TInt aInitTop, TInt aMaxSize, TOwnerType aType = EOwnerProcess) {
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

const eka2l1::hle::func_map mem_register_funcs = {
    BRIDGE_REGISTER(2758081926, RChunkCreateDisconnectLocal),
    BRIDGE_REGISTER(3201211097, RChunkTop),
    BRIDGE_REGISTER(3131320258, RChunkBase),
    BRIDGE_REGISTER(376494078, RChunkBottom),
    BRIDGE_REGISTER(762561902, RChunkCommit),
    BRIDGE_REGISTER(2317460249, RChunkDecommit),
    BRIDGE_REGISTER(3839845899, RChunkMaxSize)
}