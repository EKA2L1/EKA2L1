/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <dispatch/libraries/register.h>
#include <cstdint>

namespace eka2l1::dispatch {
    static patch_info SYSUTILS_PATCH_EPOCV81A_INFOS[] = {
        // Dispatch number, Ordinal number
        { 0x1000, 7 }
    };

    static const std::uint32_t SYSUTILS_PATCH_EPOCV81A_COUNT = sizeof(SYSUTILS_PATCH_EPOCV81A_INFOS) / sizeof(patch_info);
}