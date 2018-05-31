// EKA2l1 project
// Copyright (C) 2018 EKA2l1 team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include "layout_args.h"
#include "bridge_types.h"

#include <arm/jit_factory.h>

namespace eka2l1 {
    namespace hle {
        template <typename T>
        std::enable_if_t<sizeof(T) <= 4> write_to_gpr(arm::jitter &cpu, const arg_layout &arg, const T& data) {
            cpu->set_reg(arg.offset, static_cast<uint32_t>(data));
        }

        template <typename T>
        std::enable_if_t<sizeof(T) == 8> read_from_gpr(arm::jitter &cpu, const arg_layout &arg, const T& data) {
            cpu->set_reg(arg.offset, static_cast<uint32_t>(data));
            cpu->set_reg(arg.offset + 1, data >> 32);
        }

        template <typename T>
        void write_to_fpr(arm::jitter& cpu, const arg_layout& arg) {
            LOG_WARN("Writing to FPR unimplemented");
            return T{};
        }

        template <typename T>
        void write_to_stack(arm::jitter& cpu, const arg_layout &layout, memory_system *mem, const T& data) {
            const address sp = cpu->get_stack_top();
            const address stack_arg_offset = sp - layout.offset;

            memcpy(ptr<void>(stack_arg_offset).get(mem), &data, sizeof(T));
            cpu->set_sp(stack_arg_offset);
        }

        template <typename T>
        void write(arm::jitter& cpu, const arg_layout &layout, memory_system *mem, const T& val) {
            switch (layout.loc) {
            case arg_where::stack:
                return write_to_stack<T>(cpu, layout, mem, val);

            case arg_where::gpr:
                return write_to_gpr<T>(cpu, layout, val);

            case arg_where::fpr:
                return write_to_fpr<T>(cpu, layout, val);
            }

            return T{};
        }

        template <typename arg, size_t idx, typename... args>
        void write(arm::jitter &cpu, const args_layout<args...> &args, const memory_system* mem, const arg &val) {
            write<arg>(cpu, args[idx], mem, host_to_arm(val));
        }
    }
}