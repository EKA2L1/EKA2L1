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

#include <epoc9/base.h>

BRIDGE_FUNC(TUint, RHandleBaseAttributes, eka2l1::ptr<RHandleBase> aThis) {
    return 0x4;
}

BRIDGE_FUNC(TInt, RHandleBaseBTraceId, eka2l1::ptr<RHandleBase> aThis) {
    // The trace are unimplemented right now
    return 0;
}

BRIDGE_FUNC(void, RHandleBaseClose, eka2l1::ptr<RHandleBase> aThis) {
    eka2l1::kernel_system *kern = sys->get_kernel_system();
    RHandleBase *handle_base = aThis.get(sys->get_memory_system());

    if (!handle_base) {
        return;
    }

    // If the kernel can close
    if (kern->get_closeable(handle_base->iHandle))
        kern->close(handle_base->iHandle);
}

BRIDGE_FUNC(void, RHandleBaseSetHandle, eka2l1::ptr<RHandleBase> aThis, TInt aNewHandle) {
    RHandleBase *handle_base = aThis.get(sys->get_memory_system());

    if (!handle_base) {
        return;
    }

    // Close this kernel handle before lending it a new one
    if (sys->get_kernel_system()->get_closeable(handle_base->iHandle)) {
        RHandleBaseClose(sys, aThis);
        handle_base->iHandle = aNewHandle;
    } else {
        return;
    }
}

BRIDGE_FUNC(void, RHandleBaseSetHandleNC, eka2l1::ptr<RHandleBase> aThis, TInt aNewHandle) {
    RHandleBase *handle_base = aThis.get(sys->get_memory_system());
    eka2l1::kernel_system *kern = sys->get_kernel_system();

    if (!handle_base) {
        return;
    }

    // Close this kernel handle before lending it a new one
    if (kern->get_closeable(handle_base->iHandle)) {
        RHandleBaseClose(sys, aThis);
        handle_base->iHandle = aNewHandle;
        kern->set_closeable(handle_base->iHandle, false);
    }
    else {
        return;
    }
}


const eka2l1::hle::func_map base_register_funcs = {
    BRIDGE_REGISTER(811469717, RHandleBaseAttributes),
    BRIDGE_REGISTER(2771218285, RHandleBaseClose),
    BRIDGE_REGISTER(3005958848, RHandleBaseSetHandleNC)
};