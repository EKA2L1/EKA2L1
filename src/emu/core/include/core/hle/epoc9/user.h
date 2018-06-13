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

// Implementing all USER calls

#include <epoc9/types.h>
#include <epoc9/err.h>

#include <core.h>

#include <hle/bridge.h>
#include <ptr.h>

// Get the current thread local data
eka2l1::kernel::thread_local_data &current_local_data(eka2l1::system *sys);

BRIDGE_FUNC(TInt, UserIsRomAddress, eka2l1::ptr<TBool> aBool, eka2l1::ptr<TAny> aAddr);
BRIDGE_FUNC(void, UserExit, TInt aReason);
BRIDGE_FUNC(void, UserDbgMarkStart, TInt aCode);
BRIDGE_FUNC(eka2l1::ptr<TAny>, UserAllocZL, TInt aSize);
BRIDGE_FUNC(eka2l1::ptr<TAny>, UserAllocZ, TInt aSize);
BRIDGE_FUNC(eka2l1::ptr<TAny>, UserAlloc, TInt aSize);
BRIDGE_FUNC(void, UserFree, eka2l1::ptr<TAny> aPtr);
BRIDGE_FUNC(void, UserSetTrapHandler, eka2l1::ptr<TAny> aPtr);
BRIDGE_FUNC(eka2l1::ptr<TAny>, UserTrapHandler);
BRIDGE_FUNC(eka2l1::ptr<TAny>, UserMarkCleanupStack);
BRIDGE_FUNC(void, UserUnmarkCleanupStack, eka2l1::ptr<TAny>);
BRIDGE_FUNC(TInt, UserParameterLength, TInt aSlot);

extern const eka2l1::hle::func_map user_register_funcs;