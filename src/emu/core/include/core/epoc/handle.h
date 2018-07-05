#pragma once

#include <core.h>
#include <cstdint>

namespace eka2l1::epoc {
    enum TOwnerType {
        EOwnerProcess,
        EOwnerThread
    };

    struct RHandleBase {
        int iHandle;

        RHandleBase(int aHandle)
            : iHandle(aHandle) {}

        eka2l1::kernel_obj_ptr GetKObject(eka2l1::system * sys);
    };

    struct RProcess {
        uint32_t iAppId;

        eka2l1::process_ptr GetProcess(eka2l1::system *sys);
    };
}