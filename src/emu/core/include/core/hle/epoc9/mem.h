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

#include <epoc9/base.h>
#include <epoc9/des.h>
#include <epoc9/types.h>
#include <epoc9/char.h>

#include <hle/bridge.h>

#include <ptr.h>

struct RChunk : public RHandleBase {
    enum TRestrictions {
        EPreventAdjust
    };
};

struct TChunkCreateInfo {
    TUint iAttributes;
    TUint8 iClearByte;
    TBool iGlobal;
    TInt iInitialBottom;
    TInt iInitialTop;
    TInt iMaxSize;
    eka2l1::ptr<TDesC> iName;   // TODO: Replace with TDesC
    TOwnerType iOwnerType;
    TUint8 iSpare1;
    TUint8 iSpare2;
    TUint  iType;
    TUint  iVersion;
};

enum TChunkCreateAtt {
    ENormal = 0x0,
    EDoubleEnded = 0x1,
    EDisconnected = 0x2,
    ECache = 0x3,
    EMappingMask = 0xf,
    ELocal = 0x0,
    EGlobal = 0x10,
    EData = 0x0,
    ECode = 0x20,
    EMemNotOwned = 0x40,
    ELocalNamed = 0x80,
    EReadOnly = 0x100,
    EPagingUnspec = 0x0,
    EUnpaged = 0x40000000,
    EPaged = 0x80000000,
    EPagingMask = EPaged | EUnpaged,
    EChunkCreateAttMask = EMappingMask | EGlobal | ECode | ELocalNamed
};

BRIDGE_FUNC(TInt, RChunkAdjust, eka2l1::ptr<RChunk> aThis, TInt aNewSize);
BRIDGE_FUNC(TInt, RChunkAdjustDoubleEnded, eka2l1::ptr<RChunk> aThis, TInt aNewBottom, TInt aNewTop);
BRIDGE_FUNC(TInt, RChunkAllocate, eka2l1::ptr<RChunk> aThis, TInt aSize);
BRIDGE_FUNC(TInt, RChunkCommit, eka2l1::ptr<RChunk> aThis, TInt aOffset, TInt aSize);
BRIDGE_FUNC(TInt, RChunkDecommit, eka2l1::ptr<RChunk> aThis, TInt aOffset, TInt aSize);
BRIDGE_FUNC(eka2l1::ptr<TUint8>, RChunkBase, eka2l1::ptr<RChunk> aThis);
BRIDGE_FUNC(TInt, RChunkBottom, eka2l1::ptr<RChunk> aThis);
BRIDGE_FUNC(TInt, RChunkTop, eka2l1::ptr<RChunk> aThis);
BRIDGE_FUNC(TInt, RChunkMaxSize, eka2l1::ptr<RChunk> aThis);
BRIDGE_FUNC(TInt, RChunkCreate, eka2l1::ptr<RChunk> aThis, eka2l1::ptr<TChunkCreateInfo> aInfo);
BRIDGE_FUNC(TInt, RChunkCreateDisconnectLocal, eka2l1::ptr<RChunk> aThis, TInt aInitBottom, TInt aInitTop, TInt aMaxSize, TOwnerType aType = EOwnerProcess);

BRIDGE_FUNC(void, MemFill, eka2l1::ptr<TAny> aTrg, TInt aLen, TChar aChar);

extern const eka2l1::hle::func_map mem_register_funcs;