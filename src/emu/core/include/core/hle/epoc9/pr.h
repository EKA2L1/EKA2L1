#pragma once

#include <epoc9/base.h>
#include <epoc9/err.h>
#include <epoc9/types.h>

#include <hle/bridge.h>
#include <ptr.h>

struct RProcess : public RHandleBase {};

BRIDGE_FUNC(void, RProcessType, eka2l1::ptr<RProcess> aProcess);

extern const eka2l1::hle::func_map pr_register_funcs;