/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <core/hle/bridge.h>

namespace eka2l1 {
    namespace hle {
        template <typename... args>
        void set_vptr_cpu(memory_system *mem, arm::jitter &jitter, uint32_t* vtab, uint32_t idx, args... func_args) {
            uint32_t addr = vtab[idx];
            constexpr args_layout<args...> layouts = lay_out<typename bridge_type<args>::arm_type...>();

            write_args<args...>(jitter, layouts, std::index_sequence_for<args...>(), mem, func_args...);

            jitter->set_lr(jitter->get_pc());
            jitter->set_pc(addr);
        }
    }
}