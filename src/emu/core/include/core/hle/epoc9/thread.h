// EKA2l1 project
// Copyright (C) 2018 EKA2l1 team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <hle/bridge.h>
#include <epoc9/types.h>

#include <ptr.h>

typedef void*(*TThreadFunction)(int);

struct SThreadCreateInfo {
    TAny*           iHandle;
    TInt            iType;
    TThreadFunction iFunction;
    TAny*           iPtr;
    TAny*           iSupervisorStack;
    TInt            iSupervisorStackSize;
    TAny*           iUserStack;
    TInt            iUserStackSize;
    TInt            iInitThreadPriority;
    TInt            iInitThreadPriority;
    TPtrC           iName;
    TInt            iTotalSize;
};

struct SStdEpocThreadCreateInfo : public SThreadCreateInfo {
    TAny iAllocator;
    TInt iHeapInitialSize;
    TInt iHeapMaxSize;
    TInt iPadding;
};

BRIDGE_FUNC(TInt, UserHeapSetupThreadHeap, TBool first, eka2l1::ptr<SStdEpocThreadCreateInfo> info);