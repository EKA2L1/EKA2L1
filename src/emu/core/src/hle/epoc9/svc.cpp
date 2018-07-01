#include <epoc9/dll.h>
#include <epoc9/svc.h>
#include <epoc9/user.h>

#include <common/cvt.h>
#include <common/path.h>

#ifdef WIN32
#include <Windows.h>
#endif

#include <chrono>
#include <ctime>

BRIDGE_FUNC(eka2l1::ptr<void>, Heap) {
    auto local_data = current_local_data(sys);
    return local_data.heap;
}

BRIDGE_FUNC(eka2l1::ptr<void>, HeapSwitch, eka2l1::ptr<void> aNewHeap) {
    auto &local_data = current_local_data(sys);
    decltype(aNewHeap) old_heap = local_data.heap;
    local_data.heap = aNewHeap;

    return old_heap;
}

BRIDGE_FUNC(eka2l1::ptr<void>, TrapHandler) {
    auto local_data = current_local_data(sys);
    return local_data.trap_handler;
}

BRIDGE_FUNC(eka2l1::ptr<void>, SetTrapHandler, eka2l1::ptr<void> aNewHandler) {
    auto &local_data = current_local_data(sys);
    decltype(aNewHandler) old_handler = local_data.trap_handler;
    local_data.trap_handler = aNewHandler;

    return local_data.trap_handler;
}

BRIDGE_FUNC(eka2l1::ptr<void>, ActiveScheduler) {
    auto local_data = current_local_data(sys);
    return local_data.scheduler;
}

BRIDGE_FUNC(void, SetActiveScheduler, eka2l1::ptr<void> aNewScheduler) {
    auto &local_data = current_local_data(sys);
    local_data.scheduler = aNewScheduler;
}

BRIDGE_FUNC(void, ProcessFilename, eka2l1::ptr<RProcess> aProcess, eka2l1::ptr<TDes8> aDes) {
    memory_system *mem = sys->get_memory_system();
    kernel_system *kern = sys->get_kernel_system();

    process_ptr pr_real;

    // 0xffff8000 is a kernel mapping for current process
    if (aProcess.ptr_address() != 0xffff8000) {
        // Unlike Symbian, process is not a kernel object here
        // Its handle contains the process's uid
        pr_real = kern->get_process(aProcess.ptr_address());
    } else {
        pr_real = kern->get_process(kern->crr_process());
    }

    if (!pr_real) {
        LOG_ERROR("SvcProcessFileName: Invalid process");
        return;
    }

    TDes8 *des = aDes.get(mem);
    des->Assign(sys, pr_real->name());
}

BRIDGE_FUNC(void, ProcessType, address pr, eka2l1::ptr<TUidType> uid_type) {
    memory_system *mem = sys->get_memory_system();
    kernel_system *kern = sys->get_kernel_system();

    process_ptr pr_real;

    // 0xffff8000 is a kernel mapping for current process
    if (pr != 0xffff8000) {
        // Unlike Symbian, process is not a kernel object here
        // Its handle contains the process's uid
        pr_real = kern->get_process(pr);
    } else {
        pr_real = kern->get_process(kern->crr_process());
    }

    if (!pr_real) {
        LOG_ERROR("SvcProcessType: Invalid process");
        return;
    }

    TUidType *type = uid_type.get(mem);
    auto &tup = pr_real->get_uid_type();

    // Silento
    type->uid1 = std::get<0>(tup);
    type->uid2 = std::get<1>(tup);
    type->uid3 = std::get<2>(tup);
}

BRIDGE_FUNC(eka2l1::ptr<void>, DllTls, TInt aHandle, TInt aDllUid) {
    eka2l1::kernel::thread_local_data dat = current_local_data(sys);

    for (const auto &tls : dat.tls_slots) {
        if (tls.handle == aHandle) {
            return tls.ptr;
        }
    }

    return eka2l1::ptr<void>(nullptr);
}

BRIDGE_FUNC(TInt, DllSetTls, TInt aHandle, TInt aDllUid, eka2l1::ptr<void> aPtr) {
    eka2l1::kernel::tls_slot *slot = get_tls_slot(sys, aHandle);

    if (!slot) {
        return KErrNoMemory;
    }

    slot->ptr = aPtr;

    return KErrNone;
}

BRIDGE_FUNC(void, DllFreeTLS, TInt iHandle) {
    thread_ptr thr = sys->get_kernel_system()->crr_thread();
    thr->close_tls_slot(*thr->get_tls_slot(iHandle, iHandle));
}

BRIDGE_FUNC(TInt, SvcE8Stub) {
    return 0;
}

BRIDGE_FUNC(TInt, UTCOffset) {
    // Stubbed
    return -14400;
}

/* Unfortunately I have to implement this through an SVC call, as they generate two version of the functon points 
  to the same SVC call. Still keep the original HLE implementation
*/
BRIDGE_FUNC(TInt, SessionSendSync, TInt aHandle, TInt aOrd, eka2l1::ptr<TAny> aIpcArgs,
    eka2l1::ptr<TInt> aStatus) {
    memory_system *mem = sys->get_memory_system();
    kernel_system *kern = sys->get_kernel_system();

    // Dispatch the header
    ipc_arg arg;
    TInt *arg_header = aIpcArgs.cast<TInt>().get(mem);

    for (uint8_t i = 0; i < 4; i++) {
        arg.args[i] = *arg_header++;
    }

    arg.flag = *arg_header & (((1 << 12) - 1) | (int)ipc_arg_pin::pin_mask);

    session_ptr ss = kern->get_session(aHandle);

    if (!ss) {
        return KErrBadHandle;
    }

    return ss->send_receive_sync(aOrd, arg, aStatus.get(mem));
}

BRIDGE_FUNC(void, WaitForAnyRequest) {
    LOG_INFO("Requested to wait for request");
    sys->get_kernel_system()->crr_thread()->wait_for_any_request();
}

const eka2l1::hle::func_map svc_register_funcs = {
    /* FAST EXECUTIVE CALL */
    BRIDGE_REGISTER(0x00800000, WaitForAnyRequest),
    BRIDGE_REGISTER(0x00800001, Heap),
    BRIDGE_REGISTER(0x00800002, HeapSwitch),
    BRIDGE_REGISTER(0x00800005, ActiveScheduler),
    BRIDGE_REGISTER(0x00800006, SetActiveScheduler),
    BRIDGE_REGISTER(0x00800008, TrapHandler),
    BRIDGE_REGISTER(0x00800009, SetTrapHandler),
    BRIDGE_REGISTER(0x00800019, UTCOffset),
    /* SLOW EXECUTIVE CALL */
    BRIDGE_REGISTER(0x16, ProcessFilename),
    BRIDGE_REGISTER(0x4e, DllTls),
    BRIDGE_REGISTER(0x64, ProcessType),
    BRIDGE_REGISTER(0x76, DllSetTls),
    BRIDGE_REGISTER(0x77, DllFreeTLS),
    BRIDGE_REGISTER(0xE8, SvcE8Stub),
    BRIDGE_REGISTER(0x4D, SessionSendSync)
};
