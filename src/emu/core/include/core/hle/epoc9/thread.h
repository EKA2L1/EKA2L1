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

#include <hle/bridge.h>
#include <epoc9/types.h>

#include <ptr.h>

typedef void(*TThreadFunction)(int);

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
    TPtrC           iName;
    TInt            iTotalSize;
};

struct SStdEpocThreadCreateInfo : public SThreadCreateInfo {
    TAny* iAllocator;
    TInt  iHeapInitialSize;
    TInt  iHeapMaxSize;
    TInt  iPadding;
};

BRIDGE_FUNC(TInt, UserHeapSetupThreadHeap, TBool first, eka2l1::ptr<SStdEpocThreadCreateInfo> info);

extern const eka2l1::hle::func_map thread_register_funcs;
