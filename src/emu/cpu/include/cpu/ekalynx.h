// Copyright (c) 2026 EKA2L1 Team. Licensed under GPLv2 or later.
#pragma once

#include <cpu/arm_interface.h>

namespace eka2l1::arm {
    std::unique_ptr<core> make_ekalynx_interpreter(exclusive_monitor *monitor);
}
