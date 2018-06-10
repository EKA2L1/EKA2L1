#pragma once

#include <hle/bridge.h>

BRIDGE_FUNC(void, TCppRTExceptionsGlobalsConstructor, eka2l1::ptr<void> aThis);

extern const eka2l1::hle::func_map trap_register_funcs;