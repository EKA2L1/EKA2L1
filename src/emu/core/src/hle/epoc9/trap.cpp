#include <epoc9/trap.h>

BRIDGE_FUNC(void, TCppRTExceptionsGlobalsConstructor, eka2l1::ptr<void> aThis) {

}

const eka2l1::hle::func_map trap_register_funcs = {
    BRIDGE_REGISTER(2922720483, TCppRTExceptionsGlobalsConstructor)
};