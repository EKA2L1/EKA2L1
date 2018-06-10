#include <epoc9/allocator.h>
#include <epoc9/user.h>
#include <epoc9/des.h>
#include <epoc9/e32loader.h>

#include <common/cvt.h>

#include <core_mem.h>

// Get the current thread local data
eka2l1::kernel::thread_local_data &current_local_data(eka2l1::system *sys) {
    return sys->get_kernel_system()->crr_thread()->get_local_data();
}

BRIDGE_FUNC(TInt, UserIsRomAddress, eka2l1::ptr<TBool> aBool, eka2l1::ptr<TAny> aAddr) {
    TBool *check = aBool.get(sys->get_memory_system());
    address ptr_addr = aAddr.ptr_address();

    if (!check) {
        return KErrGeneral;
    }

    if (eka2l1::ROM < ptr_addr && ptr_addr < eka2l1::GLOBAL_DATA) {
        *check = true;
    } else {
        check = false;
    }

    return KErrNone;
}

BRIDGE_FUNC(void, UserExit, TInt aReason) {
    LOG_WARN("Exit requested from HLE side, reason: {}", aReason);

    // Kill the thread!!!!!
    eka2l1::thread_ptr crr_thread = sys->get_kernel_system()->crr_thread();
    crr_thread->stop();
    sys->get_kernel_system()->close_thread(crr_thread->unique_id());
}

BRIDGE_FUNC(void, UserPanic, ptr<TDesC16> aReasonMsg, TInt aCode) {
    hle::lib_manager *mngr = sys->get_lib_manager();
    memory_system *mem = sys->get_memory_system();

    TDesC16 *des = aReasonMsg.get(mem);
    TUint16 *reason = GetTDes16Ptr(sys, des);
    LOG_WARN("User paniced from HLE side with message: {}, code: {}", common::ucs2_to_utf8(std::u16string(reason, reason + des->iLength)), aCode);
}

const TInt KModuleEntryReasonProcessDetach = 3;

BRIDGE_FUNC(void, UserInitProcess) {
    TUint count = 0xFFFFFFFF;
    std::vector<address> addr(0x50);
    E32LoaderStaticCallList(sys, count, addr.data());

    for (uint32_t i = 0; i < count - 1; i++) {
        if (addr[i] >= 0) {
            hle::call_lle_void(sys, addr[i], KModuleEntryReasonProcessDetach);
        }
    }
}

BRIDGE_FUNC(void, UserDbgMarkStart, TInt aCode) {
    eka2l1::kernel::thread_local_data local_data = current_local_data(sys);
    RAllocatorDbgMarkStart(sys, eka2l1::ptr<RAllocator>(local_data.heap.ptr_address()));
}

BRIDGE_FUNC(eka2l1::ptr<TAny>, UserAllocZ, TInt aSize) {
    eka2l1::kernel::thread_local_data local_data = current_local_data(sys);
    auto ret = RHeapAllocZ(sys, local_data.heap.cast<RHeap>(), aSize);

    return ret;
}

BRIDGE_FUNC(eka2l1::ptr<TAny>, UserAlloc, TInt aSize) {
    eka2l1::kernel::thread_local_data local_data = current_local_data(sys);
    auto ret = RHeapAlloc(sys, local_data.heap.cast<RHeap>(), aSize);

    return ret;
}

BRIDGE_FUNC(eka2l1::ptr<TAny>, UserAllocZL, TInt aSize) {
    eka2l1::kernel::thread_local_data local_data = current_local_data(sys);
    auto ret = RHeapAllocZL(sys, local_data.heap.cast<RHeap>(), aSize);

    return ret;
}

BRIDGE_FUNC(void, UserFree, eka2l1::ptr<TAny> aPtr) {
    eka2l1::kernel::thread_local_data local_data = current_local_data(sys);
    RHeapFree(sys, local_data.heap.cast<RHeap>(), aPtr);
}

BRIDGE_FUNC(void, UserSetTrapHandler, eka2l1::ptr<TAny> aPtr) {
    eka2l1::kernel::thread_local_data &local_data = current_local_data(sys);
    local_data.trap_handler = aPtr;
}

BRIDGE_FUNC(eka2l1::ptr<TAny>, UserTrapHandler) {
    eka2l1::kernel::thread_local_data local_data = current_local_data(sys);
    return local_data.trap_handler;
}

BRIDGE_FUNC(eka2l1::ptr<TAny>, UserMarkCleanupStack) {
    return UserTrapHandler(sys);
}

BRIDGE_FUNC(void, UserUnmarkCleanupStack, eka2l1::ptr<TAny> aTrapHandler) {

}

const eka2l1::hle::func_map user_register_funcs = {
    BRIDGE_REGISTER(3511550552, UserIsRomAddress),
    BRIDGE_REGISTER(3037667387, UserExit),
    BRIDGE_REGISTER(3475190555, UserPanic),
    BRIDGE_REGISTER(3108163311, UserInitProcess),
    BRIDGE_REGISTER(1932818422, UserDbgMarkStart)
};