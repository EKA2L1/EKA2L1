#include <epoc9/hal.h>

BRIDGE_FUNC(void, UserHalPageSizeInBytes, ptr<TInt> aPageSize) {
    memory_system *mem = sys->get_memory_system();

    if (!sys) {
        return;
    }

    TInt *page_size = aPageSize.get(mem);
    *page_size = mem->get_page_size();
}

const eka2l1::hle::func_map hal_register_funcs = {
    BRIDGE_REGISTER(121380819, UserHalPageSizeInBytes)
};