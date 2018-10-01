/*
* Copyright (c) 2018 Vita3K Team / EKA2L1 Team.
*
* This file is part of Vita3K emulator project / EKA2L1 project
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

#include <common/log.h>
#include <core/hle/arg_layout.h>

#include <tuple>

namespace eka2l1 {
    namespace hle {
        struct layout_args_state {
            size_t gpr_used;
            size_t stack_used;
            size_t float_used;
        };

        constexpr size_t align(size_t current, size_t alignment) {
            return (current + (alignment - 1)) & ~(alignment - 1);
        }

        template <typename arg>
        constexpr std::tuple<arg_layout, layout_args_state> add_to_stack(const layout_args_state &state) {
            const size_t stack_alignment = alignof(arg);
            const size_t stack_required = sizeof(arg);

            const size_t stack_offset = align(state.stack_used, stack_alignment);
            const size_t next_stack_used = stack_offset + stack_required;

            const arg_layout layout = { arg_where::stack, stack_offset };
            const layout_args_state next_state = { state.gpr_used, next_stack_used, state.float_used };

            return { layout, next_state };
        }
     
        template <typename arg>
        constexpr std::tuple<arg_layout, layout_args_state> add_to_gpr_or_stack(const layout_args_state &state) {
            const size_t gpr_required = (sizeof(arg) + 3) / 4;   // r0 - r3, and stack
            const size_t gpr_alignment = gpr_required;
            const size_t gpr_idx = align(state.gpr_used, gpr_alignment);
            const size_t next_gpr_used = gpr_idx + gpr_required;

            if (next_gpr_used > 4) {
                return add_to_stack<arg>(state);
            }

            const arg_layout layout = { arg_where::gpr, gpr_idx };
            const layout_args_state next_state = { next_gpr_used, state.stack_used, state.float_used };

            return { layout, next_state };
        }

        template <typename arg>
        constexpr std::tuple<arg_layout, layout_args_state> add_to_fpr(const layout_args_state &state) {
            const size_t float_idx = state.float_used;
            const size_t next_float_used = state.float_used + 1;

            const arg_layout layout = { arg_where::fpr, float_idx };
            const layout_args_state next_state = { state.gpr_used, state.stack_used, next_float_used };

            return { layout, next_state };
        }

        template <typename arg>
        constexpr std::tuple<arg_layout, layout_args_state> add_arg_to_layout(const layout_args_state &state) {
            if constexpr(std::is_same_v<arg, float>) {
                return add_to_stack<arg>(state);
            }
            else {
                return add_to_gpr_or_stack<arg>(state);
            }
        }

        template <typename... args>
        constexpr std::enable_if_t<sizeof...(args) == 0> add_args_to_layout(arg_layout &head, layout_args_state &state) {
            //LOG_WARN("Adding arguments to layout without any arguments");
        }

        template <typename head, typename... tail>
        constexpr void add_args_to_layout(arg_layout &mhead, layout_args_state &state) {
            const std::tuple<arg_layout, layout_args_state> res = add_arg_to_layout<head>(state);
            
            mhead = std::get<0>(res);
            state = std::get<1>(res);

            add_args_to_layout<tail...>(*(&mhead + 1), state);
        }

		/*! \brief Given arguments and its types, make layout of those arguments in EABI */
        template <typename... args>
        constexpr args_layout<args...> lay_out() {
            args_layout<args...> layout = {};
            layout_args_state state = {};
            add_args_to_layout<args...>(*layout.data(), state);

            return layout;
        }
    }
}
