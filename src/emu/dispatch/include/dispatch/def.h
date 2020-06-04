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
#include <functional>
#include <bridge/bridge.h>

namespace eka2l1 {
    class system;

    namespace dispatch {
        using bridge_func = std::function<void(system *, kernel::process *, arm::core *)>;
    }
}

#define BRIDGE_FUNC_DISPATCHER(ret, name, ...) ret name(system *sys, const std::uint32_t func_num, ##__VA_ARGS__)
#define BRIDGE_REGISTER_DISPATCHER(func_sid, func)                                                  \
    {                                                                                               \
        func_sid, eka2l1::hle::bridge(&func)                                                        \
    }