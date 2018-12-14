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

#include <common/cvt.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/kernel/process.h>
#include <epoc/page_table.h>

namespace scripting = eka2l1::scripting;

namespace eka2l1::scripting {
    process::process(uint64_t handle)
        : process_handle(std::move(*reinterpret_cast<eka2l1::process_ptr *>(handle))) {
    }

    bool process::read_process_memory(const size_t addr, std::vector<char> &buffer, const size_t size) {
        void *ptr = process_handle->get_ptr_on_addr_space(addr);

        if (!ptr) {
            return false;
        }

        buffer.resize(size);
        memcpy(&buffer[0], ptr, size);

        return true;
    }

    bool process::write_process_memory(const size_t addr, std::vector<char> buffer) {
        void *ptr = process_handle->get_ptr_on_addr_space(addr);

        if (!ptr) {
            return false;
        }

        memcpy(ptr, buffer.data(), buffer.size());

        return true;
    }

    std::string process::get_executable_path() {
        return common::ucs2_to_utf8(process_handle->get_exe_path());
    }

    std::string process::get_name() {
        return process_handle->name();
    }

    std::vector<std::unique_ptr<eka2l1::scripting::thread>> process::get_thread_list() {
        system *sys = get_current_instance();
        std::vector<thread_ptr> threads = sys->get_kernel_system()->get_all_thread_own_process(process_handle);

        std::vector<std::unique_ptr<scripting::thread>> script_threads;

        for (const auto &thr : threads) {
            script_threads.push_back(std::make_unique<scripting::thread>((uint64_t)(&thr)));
        }

        return script_threads;
    }

    std::vector<std::unique_ptr<scripting::process>> get_process_list() {
        system *sys = get_current_instance();
        std::vector<process_ptr> processes = sys->get_kernel_system()->get_all_processes();

        std::vector<std::unique_ptr<scripting::process>> script_processes;

        for (const auto &pr : processes) {
            script_processes.push_back(std::make_unique<scripting::process>((uint64_t)(&pr)));
        }

        return script_processes;
    }
}