#include <epoc9/user.h>
#include <core_mem.h>

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

const eka2l1::hle::func_map user_register_funcs = {
    BRIDGE_REGISTER(3511550552, UserIsRomAddress)
};