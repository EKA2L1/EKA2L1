/*
 * Copyright (c) 2020 EKA2L1 Team.
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

namespace eka2l1::epoc {
    static constexpr std::int32_t CINT32_MAX = 0x7FFFFFFF;
    static constexpr std::uint32_t CUINT32_MAX = 0xFFFFFFFF;
    static constexpr std::int32_t CINT32_MIN = static_cast<std::int32_t>(0x80000000);

    using uid = std::uint32_t;

    static constexpr uid DYNAMIC_LIBRARY_UID = 0x10000079;
    static constexpr uid EXECUTABLE_UID = 0x1000007A;
}