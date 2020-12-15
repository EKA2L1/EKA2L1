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

#include <bridge/bridge_types.h>
#include <bridge/layout_args.h>

#include <cpu/arm_factory.h>

namespace eka2l1 {
    namespace hle {
        /**
         * @brief Read an argument from the registers. 
		 * @param cpu The CPU.
		 * @param arg The layout of that argument.
		*/
        template <typename T>
        std::enable_if_t<sizeof(T) <= 4, T> read_from_gpr(arm::core *cpu, const arg_layout &arg) {
            const uint32_t reg = cpu->get_reg(arg.offset);
            return *reinterpret_cast<const T *>(&reg);
        }

        /** 
         * @brief Read an argument from the registers. 
		 * @param cpu The CPU.
		 * @param arg The layout of that argument.
		*/
        template <typename T>
        std::enable_if_t<sizeof(T) == 8, T> read_from_gpr(arm::core *cpu, const arg_layout &arg) {
            const uint64_t low = cpu->get_reg(arg.offset - 1);
            const uint64_t high = cpu->get_reg(arg.offset);

            const uint64_t all = low | (high << 32);

            return *reinterpret_cast<const T *>(&all);
        }

        /**
         * @brief Read an argument from the float registers. 
		 * @param cpu The CPU.
		 * @param arg The layout of that argument.
		*/
        template <typename T>
        T read_from_fpr(arm::core *cpu, const arg_layout &arg) {
            LOG_WARN(BRIDGE, "Reading from FPR unimplemented");
            return T{};
        }

        /**
         * @brief Read an argument from stack. 
		 * @param cpu The CPU.
		 * @param arg The layout of that argument.
		 * @param pr The process that does this call.
		*/
        template <typename T>
        T read_from_stack(arm::core *cpu, const arg_layout &layout, kernel::process *pr) {
            const address sp = cpu->get_stack_top();
            const address stack_arg_offset = sp + static_cast<address>(layout.offset);

            return *ptr<T>(stack_arg_offset).get(pr);
        }

        /**
         * @brief Read an argument.
         *  
		 * @param cpu The CPU.
		 * @param arg The layout of that argument.
		 * @param pr The process that does this call.
		*/
        template <typename T>
        T read(arm::core *cpu, const arg_layout &layout, kernel::process *pr) {
            switch (layout.loc) {
            case arg_where::stack:
                return read_from_stack<T>(cpu, layout, pr);

            case arg_where::gpr:
                return read_from_gpr<T>(cpu, layout);

            case arg_where::fpr:
                return read_from_fpr<T>(cpu, layout);
            }

            return T{};
        }

        /**
         * @brief Read an argument. 
		 * @param cpu The CPU.
		 * @param arg The layout of that argument.
		 * @param pr The process that does this call.
		*/
        template <typename arg, size_t idx, typename... args>
        arg read(arm::core *cpu, const args_layout<args...> &margs, kernel::process *pr) {
            using arm_type = typename bridge_type<arg>::arm_type;

            const arm_type bridged = read<arm_type>(cpu, margs[idx], pr);
            return bridge_type<arg>::arm_to_host(bridged, pr);
        }
    }
}
