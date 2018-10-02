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

#include <pybind11/embed.h>

#include <scripting/thread.h>
#include <scripting/process.h>

namespace scripting = eka2l1::scripting;

namespace eka2l1::scripting {
    thread::thread(uint64_t handle)
        : thread_handle(std::move(*reinterpret_cast<eka2l1::thread_ptr *>(handle))) {
    }

    std::string thread::get_name() {
        return thread_handle->name();
    }

    uint32_t thread::get_register(uint8_t index) {
        if (thread_handle->get_thread_context().cpu_registers.size() <= index) {
            throw pybind11::index_error("CPU Register Index is out of range");
        }

        return thread_handle->get_thread_context().cpu_registers[index];
    }

    uint32_t thread::get_pc() {
        return thread_handle->get_thread_context().pc;
    }

    uint32_t thread::get_lr() {
        return thread_handle->get_thread_context().lr;
    }

    uint32_t thread::get_sp() {
        return thread_handle->get_thread_context().sp;
    }

    uint32_t thread::get_cpsr() {
        return thread_handle->get_thread_context().cpsr;
    }

    int thread::get_exit_reason() {
        return thread_handle->get_exit_reason();
    }

    int thread::get_leave_depth() {
        return thread_handle->get_leave_depth();
    }

    kernel::thread_state thread::get_state() {
        return thread_handle->current_state();
    }

    kernel::thread_priority thread::get_priority() {
        return thread_handle->get_priority();
    }

    std::unique_ptr<scripting::process> thread::get_owning_process() {
        process_ptr pr = thread_handle->owning_process();
        return std::make_unique<scripting::process>((uint64_t)(
            &pr));
    }
}