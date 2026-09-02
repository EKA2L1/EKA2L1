/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <dispatch/def.h>

namespace eka2l1::dispatch::featmgr {
    BRIDGE_FUNC_DISPATCHER(void, feature_manager_initialize_lib);
    BRIDGE_FUNC_DISPATCHER(void, feature_manager_uninitialize_lib);
    BRIDGE_FUNC_DISPATCHER(std::int32_t, feature_manager_feature_supported, const std::int32_t feature_id);
}
