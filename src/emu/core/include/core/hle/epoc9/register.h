#pragma once

#include <epoc9/allocator.h>
#include <epoc9/base.h>
#include <epoc9/char.h>
#include <epoc9/cons.h>
#include <epoc9/lock.h>
#include <epoc9/mem.h>
#include <epoc9/sts.h>
#include <epoc9/thread.h>
#include <epoc9/user.h>

#include <hle/libmanager.h>

#define ADD_REGISTERS(mngr, map) mngr.import_funcs.insert(map.begin(), map.end())

void register_epoc9(eka2l1::hle::lib_manager& mngr);