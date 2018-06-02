#include <epoc9/register.h>

void register_epoc9(eka2l1::hle::lib_manager& mngr) {
    ADD_REGISTERS(mngr, thread_register_funcs);
}