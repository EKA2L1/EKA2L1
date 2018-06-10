#pragma once

#include <epoc9/types.h>
#include <epoc9/err.h>

#include <hle/bridge.h>
#include <ptr.h>

using namespace eka2l1;

BRIDGE_FUNC(TInt, E32LoaderStaticCallList, TUint &aNumEps, address *aEpList);