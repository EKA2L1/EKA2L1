#include <epoc9/svc.h>
#include <epoc9/user.h>
#include <epoc9/dll.h>

#include <common/cvt.h>
#include <common/path.h>

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
        RProcess *pr = aProcess.get(mem);

        // Unlike Symbian, process is not a kernel object here
        // Its handle contains the process's uid
        pr_real = kern->get_process(pr->iHandle);
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

const eka2l1::hle::func_map svc_register_funcs = {
    /* FAST EXECUTIVE CALL */
    BRIDGE_REGISTER(0x00800001, Heap),
    BRIDGE_REGISTER(0x00800002, HeapSwitch),
    BRIDGE_REGISTER(0x00800005, ActiveScheduler),
    BRIDGE_REGISTER(0x00800006, SetActiveScheduler),
    BRIDGE_REGISTER(0x00800008, TrapHandler),
    BRIDGE_REGISTER(0x00800009, SetTrapHandler),
    /* SLOW EXECUTIVE CALL */
    BRIDGE_REGISTER(0x16, ProcessFilename),
    BRIDGE_REGISTER(0x4e, DllTls),
    BRIDGE_REGISTER(0x76, DllSetTls)
};
