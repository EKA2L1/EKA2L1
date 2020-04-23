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

#include <hle/bridge_types.h>
#include <hle/layout_args.h>

#include <arm/arm_factory.h>

namespace eka2l1 {
    namespace hle {
        template <typename T>
        std::enable_if_t<sizeof(T) <= 4> write_to_gpr(arm::cpu &cpu, const arg_layout &arg, const T &data) {
            cpu->set_reg(arg.offset, *reinterpret_cast<const uint32_t *>(&data));
        }

        template <typename T>
        std::enable_if_t<sizeof(T) == 8> read_from_gpr(arm::cpu &cpu, const arg_layout &arg, const T &data) {
            cpu->set_reg(arg.offset, *reinterpret_cast<const uint32_t *>(&data));
            cpu->set_reg(arg.offset + 1, *reinterpret_cast<const uint64_t *>(&data) >> 32);
        }

        template <typename T>
        void write_to_fpr(arm::cpu &cpu, const arg_layout &arg) {
            LOG_WARN("Writing to FPR unimplemented");
        }

        template <typename T>
        void write_to_stack(arm::cpu &cpu, const arg_layout &layout, memory_system *mem, const T &data) {
            const address sp = cpu->get_stack_top();
            const address stack_arg_offset = sp - sizeof(T);

            memcpy(ptr<void>(stack_arg_offset).get(mem), &data, sizeof(T));
            cpu->set_sp(stack_arg_offset);
        }

        template <typename T>
        void write(arm::cpu &cpu, const arg_layout &layout, memory_system *mem, const T &val) {
            switch (layout.loc) {
            case arg_where::stack:
                write_to_stack<T>(cpu, layout, mem, val);
                break;

            case arg_where::gpr:
                write_to_gpr<T>(cpu, layout, val);
                break;

            case arg_where::fpr:
                write_to_fpr<T>(cpu, layout);
                break;
            }
        }

        template <typename arg, size_t idx, typename... args>
        void write(arm::cpu &cpu, const args_layout<args...> &margs, memory_system *mem, arg val) {
            write<arg>(cpu, margs[idx], mem, bridge_type<arg>::host_to_arm(val, mem));
        }
    }
}