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
#include <epoc9/allocator.h>
#include <epoc9/base.h>
#include <epoc9/err.h>

BRIDGE_FUNC(TInt, RAllocatorOpen, eka2l1::ptr<RAllocator> aAllocator) {
    RAllocator *allocator = aAllocator.get(sys->get_memory_system());

    if (!allocator) {
        return KErrGeneral;
    }

    ++allocator->iAccessCount;

    return KErrNone;
}


BRIDGE_FUNC(void, RAllocatorClose, eka2l1::ptr<RAllocator> aAllocator) {
    RAllocator *allocator = aAllocator.get(sys->get_memory_system());

    if (!allocator) {
        LOG_WARN("**this** is nullptr");
        return;
    }

    if (--allocator->iAccessCount <= 0) {
        LOG_WARN("Allocator closes too many times, access count is down to zero");
    }

    if (allocator->iAccessCount == 1) {
        RAllocatorDoClose(sys, aAllocator);
    }
}

BRIDGE_FUNC(void, RAllocatorDoClose, eka2l1::ptr<RAllocator> aAllocator) {
    RAllocator *allocator = aAllocator.get(sys->get_memory_system());

    if (!allocator) {
        LOG_WARN("**this** is nullptr");
        return;
    }

    TInt* handles = allocator->iHandles.get(sys->get_memory_system());

    for (uint32_t i = 0; i < allocator->iHandleCount; i++) {
        RHandleBase handleBase;
        RHandleBaseSetHandleNoHLEPointer(sys, &handleBase, handles[i]);
        RHandleBaseCloseNoHLEPointer(sys, &handleBase);
    }
}

const eka2l1::hle::func_map allocator_register_funcs = {
    BRIDGE_REGISTER(3787177057, RAllocatorOpen),
    BRIDGE_REGISTER(1629345542, RAllocatorClose),
    BRIDGE_REGISTER(381559899, RAllocatorDoClose)
};