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

#include "layout_args.h"
#include "bridge_types.h"

#include <arm/jit_factory.h>

namespace eka2l1 {
    namespace hle {
        template <typename T>
        std::enable_if_t<sizeof(T) <= 4, T> read_from_gpr(arm::jitter &cpu, const arg_layout &arg) {
            const uint32_t reg = cpu->get_reg(arg.offset);
            return static_cast<T>(reg);
        }

        template <typename T>
        std::enable_if_t<sizeof(T) == 8, T> read_from_gpr(arm::jitter &cpu, const arg_layout &arg) {
            const uint64_t low = cpu->get_reg(arg.offset);
            const uint64_t high = cpu->get_reg(arg.offset + 1);

            return static_cast<T>(low | (high << 32));
        }

        template <typename T>
        T read_from_fpr(arm::jitter& cpu, const arg_layout& arg) {
            LOG_WARN("Reading from FPR unimplemented");
            return T{};
        }

        template <typename T>
        T read_from_stack(arm::jitter& cpu, const arg_layout &layout, memory_system *mem) {
            const address sp = cpu->get_stack_top();
            const address stack_arg_offset = sp + layout.offset;

            return *ptr<T>(stack_arg_offset).get(mem);
        }

        template <typename T>
        T read(arm::jitter& cpu, const arg_layout &layout, memory_system *mem) {
            switch (layout.loc) {
                case arg_where::stack:
                    return read_from_stack<T>(cpu, layout, mem);
            
                case arg_where::gpr: 
                    return read_from_gpr<T>(cpu, layout);

                case arg_where::fpr:
                    return read_from_fpr<T>(cpu, layout);
            }

            return T{};
        }

        template <typename arg, size_t idx, typename... args>
        arg read(arm::jitter &cpu, const args_layout<args...> &margs, memory_system* mem) {
            using arm_type = typename bridge_type<arg>::arm_type;
            
            const arm_type bridged = read<arm_type>(cpu, margs[idx], mem);
            return bridge_type<arg>::arm_to_host(bridged, mem);
        }
    }
}
