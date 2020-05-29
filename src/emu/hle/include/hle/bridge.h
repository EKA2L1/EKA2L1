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

#include <hle/arg_layout.h>
#include <hle/bridge_types.h>
#include <hle/layout_args.h>
#include <hle/read_arg.h>
#include <hle/return_val.h>
#include <hle/write_arg.h>

#include <epoc/epoc.h>
#include <kernel/kernel.h>

#include <cstdint>
#include <functional>
#include <type_traits>

namespace eka2l1 {
    namespace hle {
        using import_func = std::function<void(system *)>;

        /*! \brief Call a HLE function without return value. */
        template <typename ret, typename... args, size_t... indices>
        std::enable_if_t<!std::is_same_v<ret, void>, void> call(ret (*export_fn)(system *, args...), const args_layout<args...> &layout, std::index_sequence<indices...>, arm::cpu &cpu, system *symsys) {
            kernel::process *crr_process = symsys->get_kernel_system()->crr_process();
            const ret result = (*export_fn)(symsys, read<args, indices, args...>(cpu, layout, crr_process)...);

            write_return_value(cpu, result);
        }

        /*! \brief Call a HLE function with return value. */
        template <typename... args, size_t... indices>
        void call(void (*export_fn)(system *, args...), const args_layout<args...> &layout, std::index_sequence<indices...>, arm::cpu &cpu, system *symsys) {
            kernel::process *crr_process = symsys->get_kernel_system()->crr_process();
            (*export_fn)(symsys, read<args, indices, args...>(cpu, layout, crr_process)...);
        }

        /*! \brief Bridge a HLE function to guest (ARM - Symbian). */
        template <typename ret, typename... args>
        import_func bridge(ret (*export_fn)(system *, args...)) {
            constexpr args_layout<args...> layouts = lay_out<typename bridge_type<args>::arm_type...>();

            return [export_fn, layouts](system *symsys) {
                using indices = std::index_sequence_for<args...>;
                call(export_fn, layouts, indices(), symsys->get_cpu(), symsys);
            };
        }

        /*! \brief Write function arguments to guest. */
        template <typename... args, size_t... indices>
        void write_args(arm::cpu &cpu, const std::array<arg_layout, sizeof...(indices)> &layouts, std::index_sequence<indices...>, kernel::process *pr, args... lle_args) {
            ((void)write<args, indices, args...>(cpu, layouts, pr, std::forward<args>(lle_args)), ...);
        }

        /*! \brief Call a LLE function with return value. */
        template <typename ret, typename... args>
        ret call_lle(hle::lib_manager *mngr, arm::cpu &mcpu, disasm *asmdis, kernel::process *pr, const address addr, args... lle_args) {
            constexpr args_layout<args...> layouts = lay_out<typename bridge_type<args>::arm_type...>();

            arm::arm_interface::thread_context crr_caller_context;
            mcpu->save_context(crr_caller_context);

            using indices = std::index_sequence_for<args...>;

            write_args<args...>(mcpu, layouts, indices(), pr, lle_args...);

            mcpu->set_lr(static_cast<std::uint32_t>(1ULL << 63));
            mcpu->set_pc(addr);
            mcpu->set_lr(crr_caller_context.pc);

            //arm::arm_interface::thread_context test_caller_context;
            //mcpu->save_context(crr_caller_context);

            mcpu->run(1UL << 31);
            mcpu->set_pc(crr_caller_context.pc);

            return read_return_value<ret>(mcpu);
        }

        /*! \brief Call LLE function without return value */
        template <typename... args>
        void call_lle_void(hle::lib_manager *mngr, arm::cpu &mcpu, disasm *asmdis, kernel::process *pr, const address addr, args... lle_args) {
            constexpr args_layout<args...> layouts = lay_out<typename bridge_type<args>::arm_type...>();

            arm::arm_interface::thread_context crr_caller_context;
            mcpu->save_context(crr_caller_context);

            using indices = std::index_sequence_for<args...>;

            write_args<args...>(mcpu, layouts, indices(), pr, lle_args...);

            mcpu->set_lr(static_cast<std::uint32_t>(1ULL << 63));
            mcpu->set_pc(addr);

            arm::arm_interface::thread_context test_caller_context;
            mcpu->save_context(crr_caller_context);

            mcpu->run(1UL << 31);
            mcpu->set_pc(crr_caller_context.pc);
        }

        template <typename... args>
        void call_lle_void(eka2l1::system *sys, const address addr, args... lle_args) {
            kernel::process *crr_process = sys->get_kernel_system()->crr_process();
            hle::lib_manager *mngr = sys->get_lib_manager();
            disasm *asmdis = sys->get_disasm();

            arm::cpu &cpu = sys->get_cpu();

            call_lle_void<args...>(mngr, cpu, asmdis, crr_process, addr, lle_args...);
        }

        template <typename ret, typename... args>
        ret call_lle(eka2l1::system *sys, const address addr, args... lle_args) {
            kernel::process *crr_process = sys->get_kernel_system()->crr_process();
            hle::lib_manager *mngr = sys->get_lib_manager();
            disasm *asmdis = sys->get_disasm();

            arm::cpu &cpu = sys->get_cpu();

            return call_lle<ret, args...>(mngr, cpu, asmdis, crr_process, addr, lle_args...);
        }

#define BRIDGE_REGISTER(func_sid, func)                                               \
    {                                                                                 \
        func_sid, eka2l1::hle::epoc_import_func { eka2l1::hle::bridge(&func), #func } \
    }

#define BRIDGE_FUNC(ret, name, ...) ret name(eka2l1::system *sys, ##__VA_ARGS__)
    }
}
