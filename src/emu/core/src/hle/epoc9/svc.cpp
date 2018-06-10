#include <epoc9/svc.h>
#include <epoc9/user.h>

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

const eka2l1::hle::func_map svc_register_funcs = {
    BRIDGE_REGISTER(0x00800001, Heap),
    BRIDGE_REGISTER(0x00800002, HeapSwitch),
    BRIDGE_REGISTER(0x00800005, ActiveScheduler),
    BRIDGE_REGISTER(0x00800006, SetActiveScheduler),
    BRIDGE_REGISTER(0x00800008, TrapHandler),
    BRIDGE_REGISTER(0x00800009, SetTrapHandler)
};
