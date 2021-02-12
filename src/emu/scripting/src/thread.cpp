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

#include <scripting/instance.h>
#include <scripting/process.h>
#include <scripting/thread.h>

#include <system/epoc.h>
#include <kernel/kernel.h>
#include <kernel/thread.h>

#if ENABLE_PYTHON_SCRIPTING
#include <pybind11/pybind11.h>
#endif

namespace scripting = eka2l1::scripting;

namespace eka2l1::scripting {
    thread::thread(uint64_t handle)
        : thread_handle(reinterpret_cast<eka2l1::kernel::thread *>(handle)) {
    }

    std::string thread::get_name() {
        return thread_handle->name();
    }

    std::uint32_t thread::get_stack_base() {
        return thread_handle->get_stack_chunk()->base(thread_handle->owning_process()).ptr_address();
    }

    std::uint32_t thread::get_heap_base() {
        return thread_handle->get_local_data()->heap.ptr_address();
    }

    uint32_t thread::get_register(uint8_t index) {
        if (thread_handle->get_thread_context().cpu_registers.size() <= index) {
#if ENABLE_PYTHON_SCRIPTING
            throw pybind11::index_error("CPU Register Index is out of range");
#else
            return 0xFFFFFFFF;
#endif
        }

        return thread_handle->get_thread_context().cpu_registers[index];
    }

    uint32_t thread::get_pc() {
        return thread_handle->get_thread_context().get_pc();
    }

    uint32_t thread::get_lr() {
        return thread_handle->get_thread_context().get_lr();
    }

    uint32_t thread::get_sp() {
        return thread_handle->get_thread_context().get_sp();
    }

    uint32_t thread::get_cpsr() {
        return thread_handle->get_thread_context().cpsr;
    }

    int thread::get_exit_reason() {
        return thread_handle->get_exit_reason();
    }

    int thread::get_state() {
        return static_cast<int>(thread_handle->current_state());
    }

    int thread::get_priority() {
        return static_cast<int>(thread_handle->get_priority());
    }

    std::unique_ptr<scripting::thread> thread::next_in_process() {
        kernel::thread *thr = thread_handle->next_in_process();
        if (!thr) {
            return nullptr;
        }

        return std::make_unique<scripting::thread>(reinterpret_cast<std::uint64_t>(thr));
    }

    std::unique_ptr<scripting::process> thread::get_owning_process() {
        kernel::process *pr = thread_handle->owning_process();
        return std::make_unique<scripting::process>(reinterpret_cast<uint64_t>(pr));
    }

    std::unique_ptr<eka2l1::scripting::thread> get_current_thread() {
        if (get_current_instance()->get_kernel_system()->crr_thread() == nullptr) {
            return nullptr;
        }

        return std::make_unique<scripting::thread>(reinterpret_cast<std::uint64_t>(
            get_current_instance()->get_kernel_system()->crr_thread()));
    }
}

extern "C" {
    EKA2L1_EXPORT void symemu_free_thread(eka2l1::scripting::thread *thr) {
        delete thr;
    }

    EKA2L1_EXPORT eka2l1::scripting::thread *symemu_get_current_thread() {
        eka2l1::kernel::thread *thr = eka2l1::scripting::get_current_instance()->
            get_kernel_system()->crr_thread();

        if (!thr) {
            return nullptr;
        }

        return new eka2l1::scripting::thread(reinterpret_cast<std::uint64_t>(thr));
    }

    EKA2L1_EXPORT eka2l1::scripting::thread *symemu_next_thread_in_process(eka2l1::scripting::thread *thr) {
        eka2l1::kernel::thread *nnt = thr->get_thread_handle()->next_in_process();
        if (!nnt) {
            return nullptr;
        }

        return new eka2l1::scripting::thread(reinterpret_cast<std::uint64_t>(nnt));
    }

    EKA2L1_EXPORT eka2l1::scripting::process *symemu_thread_own_process(eka2l1::scripting::thread *thr) {
        eka2l1::kernel::process *pr = thr->get_thread_handle()->owning_process();
        if (!pr) {
            return nullptr;
        }

        return new eka2l1::scripting::process(reinterpret_cast<std::uint64_t>(pr));
    }

    EKA2L1_EXPORT std::uint32_t symemu_thread_stack_base(eka2l1::scripting::thread *thr) {
        return thr->get_stack_base();
    }

    EKA2L1_EXPORT std::uint32_t symemu_thread_get_heap_base(eka2l1::scripting::thread *thr) {
        return thr->get_heap_base();
    }

    EKA2L1_EXPORT std::uint32_t symemu_thread_get_register(eka2l1::scripting::thread *thr, uint8_t index) {
        return thr->get_register(index);
    }

    EKA2L1_EXPORT uint32_t symemu_thread_get_pc(eka2l1::scripting::thread *thr) {
        return thr->get_pc();
    }

    EKA2L1_EXPORT uint32_t symemu_thread_get_lr(eka2l1::scripting::thread *thr) {
        return thr->get_lr();
    }

    EKA2L1_EXPORT uint32_t symemu_thread_get_sp(eka2l1::scripting::thread *thr) {
        return thr->get_sp();
    }

    EKA2L1_EXPORT uint32_t symemu_thread_get_cpsr(eka2l1::scripting::thread *thr) {
        return thr->get_cpsr();
    }

    EKA2L1_EXPORT int symemu_thread_get_exit_reason(eka2l1::scripting::thread *thr) {
        return thr->get_exit_reason();
    }

    EKA2L1_EXPORT int symemu_thread_current_state(eka2l1::scripting::thread *thr) {
        return thr->get_state();
    }

    EKA2L1_EXPORT int symemu_thread_priority(eka2l1::scripting::thread *thr) {
        return thr->get_priority();
    }

    EKA2L1_EXPORT const char *symemu_thread_name(eka2l1::scripting::thread *thr) {
        std::string data = thr->get_name();
        char *ret_val = new char[data.length() + 1];

        std::memcpy(ret_val, data.data(), data.length());
        ret_val[data.length()] = '\0';

        return ret_val;
    }
}