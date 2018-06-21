#pragma once

#include <epoc9/base.h>
#include <epoc9/err.h>
#include <epoc9/types.h>
#include <epoc9/des.h>

#include <hle/bridge.h>

#include <ptr.h>

struct RSessionBase: public RHandleBase {};

BRIDGE_FUNC(TInt, RSessionBaseCreateSession, eka2l1::ptr<RSessionBase> aSession, eka2l1::ptr<TDesC> aServerName,
    eka2l1::ptr<void> aVersion, TInt aAsyncMsgSlot);

extern const eka2l1::hle::func_map svr_register_funcs;