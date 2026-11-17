// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <control/api.h>
#include <common/path_types.h>

namespace eka2l1::control {
    response host_files(const request &req, const host_path &resolved);
    response host_log(const request &req, const host_path &path);
}
