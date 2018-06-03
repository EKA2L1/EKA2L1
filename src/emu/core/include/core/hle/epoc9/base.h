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
#include <hle/bridge.h>

#include <ptr.h>

struct CBase {
    void *vtable;
};

struct RHandleBase {
    TInt iHandle;
};

BRIDGE_FUNC(TUint, RHandleBaseAttributes, eka2l1::ptr<RHandleBase> aThis);
BRIDGE_FUNC(TInt, RHandleBaseBTraceId, eka2l1::ptr<RHandleBase> aThis);
BRIDGE_FUNC(void, RHandleBaseClose, eka2l1::ptr<RHandleBase> aThis);
BRIDGE_FUNC(void, RHandleBaseSetHandle, eka2l1::ptr<RHandleBase> aThis, TInt aNewHandle);
BRIDGE_FUNC(void, RHandleBaseSetHandleNC, eka2l1::ptr<RHandleBase> aThis, TInt aNewHandle);

extern const eka2l1::hle::func_map base_register_funcs;