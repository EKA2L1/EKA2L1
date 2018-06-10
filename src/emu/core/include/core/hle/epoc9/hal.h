#pragma once

#include <epoc9/types.h>
#include <epoc9/err.h>

#include <hle/bridge.h>
#include <ptr.h>

using namespace eka2l1;

BRIDGE_FUNC(void, UserHalPageSizeInBytes, ptr<TInt> aPageSize);

extern const eka2l1::hle::func_map hal_register_funcs;