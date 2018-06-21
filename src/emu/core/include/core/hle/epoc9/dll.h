#pragma once

#include <epoc9/types.h>
#include <epoc9/err.h>

#include <hle/bridge.h>

eka2l1::kernel::tls_slot *get_tls_slot(eka2l1::system *sys, address addr);