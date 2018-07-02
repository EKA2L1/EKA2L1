#include <epoc9/allocator.h>
#include <epoc9/des.h>
#include <epoc9/e32loader.h>
#include <epoc9/thread.h>
#include <epoc9/user.h>

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
    LOG_WARN("User paniced from HLE side with message: {}, code: {}", common::ucs2_to_utf8(std::u16string(reason, reason + (des->iLength & 0xfffffff))), aCode);

    kernel_system *kern = sys->get_kernel_system();
    kern->close_process(kern->crr_process());
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

BRIDGE_FUNC(TInt, UserParameterLength, TInt aSlot) {
    kernel_system *kern = sys->get_kernel_system();
    process_ptr pr = kern->get_process(kern->crr_process());

    if (aSlot >= 16 || aSlot < 0) {
        LOG_ERROR("User requested parameter slot is not available (slot: {} >= 16 or < 0)", aSlot);
        UserExit(sys, 51);

        return KErrGeneral;
    }

    auto slot = *pr->get_arg_slot(aSlot);

    if (slot.data_size == -1) {
        return KErrNotFound;
    }

    return slot.data_size;
}

BRIDGE_FUNC(void, UserLineCommand, eka2l1::ptr<TDesC> aDes) {
    TDesC *des = aDes.get(sys->get_memory_system());
    TUint16 *ptr = GetTDes16Ptr(sys, des);

    kernel_system *kern = sys->get_kernel_system();
    std::u16string cmd_args = kern->get_process(kern->crr_process())->get_cmd_args();

    memcpy(ptr, cmd_args.data(), cmd_args.length() * 2);
    SetLengthDes(sys, des, cmd_args.length());
}

BRIDGE_FUNC(TInt, UserCommandLineLength) {
    kernel_system *kern = sys->get_kernel_system();
    auto cmd_args = kern->get_process(kern->crr_process())->get_cmd_args();

    return cmd_args.length();
}

BRIDGE_FUNC(TInt, UserGetTIntParameter, TInt aSlot, ptr<TInt> val) {
    kernel_system *kern = sys->get_kernel_system();
    process_ptr pr = kern->get_process(kern->crr_process());

    if (aSlot >= 16 || aSlot < 0) {
        LOG_ERROR("User requested parameter slot is not available (slot: {} >= 16 or < 0)", aSlot);
        UserExit(sys, 51);

        return KErrGeneral;
    }

    auto slot = *pr->get_arg_slot(aSlot);

    if (slot.data_size == -1) {
        return KErrNotFound;
    }

    *val.get(sys->get_memory_system()) = pr->get_arg_slot(aSlot)->data;

    return KErrNone;
}

BRIDGE_FUNC(TInt, UserRenameThread, eka2l1::ptr<TDesC16> aNewName) {
    kernel_system *kern = sys->get_kernel_system();
    memory_system *mem = sys->get_memory_system();

    thread_ptr crr_thread = kern->crr_thread();

    TDesC16 *des = aNewName.get(mem);
    std::string new_thr_name = common::ucs2_to_utf8(des->StdString(sys));

    LOG_INFO("Thread renamed to {}", new_thr_name);

    crr_thread->rename(new_thr_name);

    return KErrNone;
}

BRIDGE_FUNC(TInt, UserGetDesParameter16, TInt aSlot, eka2l1::ptr<TDes> aDes) {
    kernel_system *kern = sys->get_kernel_system();
    process_ptr pr = kern->get_process(kern->crr_process());

    if (aSlot >= 16 || aSlot < 0) {
        LOG_ERROR("User requested parameter slot is not available (slot: {} >= 16 or < 0)", aSlot);
        UserExit(sys, 51);

        return KErrGeneral;
    }

    auto slot = *pr->get_arg_slot(aSlot);

    if (slot.data_size == -1) {
        return KErrNotFound;
    }

    TDes *des = aDes.get(sys->get_memory_system());
    TUint16 *ptr = GetTDes16Ptr(sys, des);

    if (aSlot == 1) {
        std::u16string arg = pr->get_exe_path() + u" " + pr->get_cmd_args();
        memcpy(ptr, arg.data(), slot.data_size);

        SetLengthDes(sys, des, slot.data_size / 2);
        return KErrNone;
    }

    TUint16 *ptr2 = eka2l1::ptr<TUint16>(slot.data).get(sys->get_memory_system());
    memcpy(ptr, ptr2, slot.data_size);

    SetLengthDes(sys, des, slot.data_size / 2);
    return KErrNone;
}

BRIDGE_FUNC(TInt, UserGetDesParameter, TInt aSlot, eka2l1::ptr<TDes8> aDes) {
    kernel_system *kern = sys->get_kernel_system();
    process_ptr pr = kern->get_process(kern->crr_process());

    if (aSlot >= 16 || aSlot < 0) {
        LOG_ERROR("User requested parameter slot is not available (slot: {} >= 16 or < 0)", aSlot);
        UserExit(sys, 51);

        return KErrGeneral;
    }

    auto slot = *pr->get_arg_slot(aSlot);

    if (slot.data_size == -1) {
        return KErrNotFound;
    }

    TDes8 *des = aDes.get(sys->get_memory_system());
    TUint8 *ptr = GetTDes8Ptr(sys, des);

    if (aSlot == 1) {
        std::u16string arg = u"\0l" + pr->get_exe_path();

        if (!pr->get_cmd_args().empty()) {
            arg += u" " + pr->get_cmd_args();
        }

        char src = 0x00;
        char src2 = 0x6C;

        memcpy(ptr + 2, common::ucs2_to_utf8(arg).data(), arg.length());
        memcpy(ptr, &src, 1);
        memcpy(ptr + 1, &src2, 1);

        SetLengthDes(sys, des, slot.data_size);

        return KErrNone;
    }

    TUint8 *ptr2 = eka2l1::ptr<TUint8>(slot.data).get(sys->get_memory_system());
    memcpy(ptr, ptr2, slot.data_size / 2);

    SetLengthDes(sys, des, slot.data_size);

    return KErrNone;
}

BRIDGE_FUNC(eka2l1::ptr<void>, memcpy, eka2l1::ptr<void> dest, eka2l1::ptr<void> src, TInt size) {
    memcpy(dest.get(sys->get_memory_system()), src.get(sys->get_memory_system()), size);
    auto ptr = src.get(sys->get_memory_system());

    return dest;
}

const eka2l1::hle::func_map user_register_funcs = {
    BRIDGE_REGISTER(3511550552, UserIsRomAddress),
    BRIDGE_REGISTER(3037667387, UserExit),
    BRIDGE_REGISTER(3475190555, UserPanic),
    BRIDGE_REGISTER(3108163311, UserInitProcess),
    BRIDGE_REGISTER(1932818422, UserDbgMarkStart),
    BRIDGE_REGISTER(3656744374, UserParameterLength),
    BRIDGE_REGISTER(77871723, UserCommandLineLength),
    BRIDGE_REGISTER(3535789199, UserGetTIntParameter),
    BRIDGE_REGISTER(411482431, UserGetDesParameter),
    BRIDGE_REGISTER(1985486127, UserGetDesParameter16),
    BRIDGE_REGISTER(226653584, memcpy),
    BRIDGE_REGISTER(3039785093, UserRenameThread),
    //BRIDGE_REGISTER(1727505686, UserLeaveIfError)
};