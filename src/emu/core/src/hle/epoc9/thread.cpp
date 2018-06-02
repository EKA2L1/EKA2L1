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
#include <epoc9/thread.h>

BRIDGE_FUNC(TInt, UserHeapSetupThreadHeap, TBool aFirst, eka2l1::ptr<SStdEpocThreadCreateInfo> aInfo) {
    SStdEpocThreadCreateInfo *info = aInfo.get(sys->get_memory_system());

    if (!info) {
        return -1;
    }

    // If there hasn't been any allocator for the thread yet, and heap is needed
    if (!info->iAllocator && info->iHeapInitialSize > 0) {
        // Create Thread Heap
        return 0;
    } else if (info->iAllocator) {
        // There is an allocator, switch it with the global User allocator
    }

    return 0;
}

const eka2l1::hle::func_map thread_register_funcs = {
    BRIDGE_REGISTER(4238211793, UserHeapSetupThreadHeap)
};