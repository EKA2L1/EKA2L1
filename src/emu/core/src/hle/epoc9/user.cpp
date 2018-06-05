#include <epoc9/allocator.h>
#include <epoc9/user.h>
#include <epoc9/des.h>
#include <epoc9/e32loader.h>

#include <common/cvt.h>

#include <core_mem.h>

RAllocator *allocator;

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

const eka2l1::hle::func_map user_register_funcs = {
    BRIDGE_REGISTER(3511550552, UserIsRomAddress),
    BRIDGE_REGISTER(3037667387, UserExit),
    BRIDGE_REGISTER(3475190555, UserPanic),
    BRIDGE_REGISTER(3108163311, UserInitProcess)
};