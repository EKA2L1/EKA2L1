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

#include <common/types.h>

#include <bridge/arg_layout.h>
#include <bridge/bridge_types.h>
#include <bridge/layout_args.h>
#include <bridge/read_arg.h>
#include <bridge/return_val.h>
#include <bridge/write_arg.h>

#include <cstdint>
#include <functional>
#include <type_traits>

namespace eka2l1 {
    namespace hle {
        /*! \brief Call a HLE function without return value. */
        template <typename T, typename ret, typename... args, size_t... indices>
        std::enable_if_t<!std::is_same_v<ret, void>, void> call(ret (*export_fn)(T *, args...), const args_layout<args...> &layout, std::index_sequence<indices...>, arm::core *cpu, kernel::process *pr, T *data) {
            const ret result = (*export_fn)(data, read<args, indices, args...>(cpu, layout, pr)...);
            write_return_value(cpu, result);
        }

        /*! \brief Call a HLE function with return value. */
        template <typename T, typename... args, size_t... indices>
        void call(void (*export_fn)(T*, args...), const args_layout<args...> &layout, std::index_sequence<indices...>, arm::core *cpu, kernel::process *pr, T *data) {
            (*export_fn)(data, read<args, indices, args...>(cpu, layout, pr)...);
        }

        /*! \brief Bridge a HLE function to guest (ARM - Symbian). */
        template <typename T, typename ret, typename... args>
        auto bridge(ret (*export_fn)(T *, args...)) {
            constexpr args_layout<args...> layouts = lay_out<typename bridge_type<args>::arm_type...>();

            return [export_fn, layouts](T *data, kernel::process *pr, arm::core *cpu) {
                using indices = std::index_sequence_for<args...>;
                call(export_fn, layouts, indices(), cpu, pr, data);
            };
        }
    }
}
