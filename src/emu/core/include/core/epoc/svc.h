// Header for resolving SVC call for function which we don't want to implement.
// This exists because of many Symbian devs make their own allocator and switch it
// Some functions are overriden with HLE functions instead of just implementing the svc call
// Because of its complex structure (NMutex, NThread) change over different Symbian update
// But, the public structure still the same for them (RThread, RFastLock), contains only
// a handle. This give us advantage.

#pragma once

#include <epoc/des.h>
#include <epoc/handle.h>

#include <hle/bridge.h>

#include <common/e32inc.h>
#include <e32err.h>

namespace eka2l1::epoc {
    BRIDGE_FUNC(eka2l1::ptr<void>, Heap);
    BRIDGE_FUNC(eka2l1::ptr<void>, HeapSwitch, eka2l1::ptr<void> aNewHeap);
    BRIDGE_FUNC(eka2l1::ptr<void>, TrapHandler);
    BRIDGE_FUNC(eka2l1::ptr<void>, SetTrapHandler, eka2l1::ptr<void> aNewHandler);
    BRIDGE_FUNC(eka2l1::ptr<void>, ActiveScheduler);
    BRIDGE_FUNC(void, SetActiveScheduler, eka2l1::ptr<void> aNewScheduler);
    BRIDGE_FUNC(void, ProcessFilename, TInt aProcessHandle, eka2l1::ptr<epoc::TDes8> aDes);
    BRIDGE_FUNC(eka2l1::ptr<void>, DllTls, TInt aHandle, TInt aDllUid);
    BRIDGE_FUNC(TInt, DllSetTls, TInt aHandle, TInt aDllUid, eka2l1::ptr<void> aPtr);

    BRIDGE_FUNC(TInt, SessionSendSync, TInt aHandle, TInt aOrd, eka2l1::ptr<TAny> aIpcArgs,
        eka2l1::ptr<TInt> aStatus);

    extern const eka2l1::hle::func_map svc_register_funcs_v93;
    extern const eka2l1::hle::func_map svc_register_funcs_v94;
}
