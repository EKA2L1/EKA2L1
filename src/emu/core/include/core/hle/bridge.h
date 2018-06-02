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

#include <hle/bridge_types.h>
#include <hle/layout_args.h>
#include <hle/read_arg.h>
#include <hle/write_arg.h>
#include <hle/return_val.h>
#include <hle/arg_layout.h>

#include <core.h>

#include <cstdint>
#include <functional>

namespace eka2l1 {
    class system;

    namespace kernel {
        using uid = uint64_t;
    }

    namespace hle {
        using import_func = std::function<void(system*)>;

        template <typename ret, typename... args, size_t... indices>
        void call(ret(*export_fn)(system*, args...), const args_layout<args...> &layout, std::index_sequence<indices...>, arm::jitter& cpu, system* symsys) {
            const ret result = (*export_fn)(symsys, read<args, indices, args...>(cpu, layout, symsys->get_memory_system())...);

            write_return_value(cpu, result);
        }

        template <typename... args, size_t... indices>
        void call(void(*export_fn)(system*, args...), const args_layout<args...> &layout, std::index_sequence<indices...>, arm::jitter& cpu, system* symsys) {
            (*export_fn)(symsys, read<arg, indices, args...>(cpu, layout, symsys->get_memory_system())...);
        }

        template <typename ret, typename... args>
        import_func bridge(ret(*export_fn)(system*, args...)) {
            constexpr args_layout<args...> layouts = lay_out<typename bridge_type<args>::arm_type...>();

            return [export_fn, layouts](system* symsys) {
                using indices = std::index_sequence_for<args...>;
                call(export_fn, layouts, indices(), symsys->get_cpu(), symsys);
            };
        }

        #define BRIDGE_REGISTER(func_sid, func) { func_sid, eka2l1::hle::bridge(&func) }
        #define BRIDGE_FUNC(ret, name, ...) ret name(eka2l1::system *sys, ##__VA_ARGS__)

        using func_map = std::unordered_map<uint32_t, import_func>;
    }
}
