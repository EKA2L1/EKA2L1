// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <control/api.h>

namespace eka2l1::control {
    response host_files(const request &req, const std::string &resolved);
    response host_log(const request &req, const std::string &path);
}
