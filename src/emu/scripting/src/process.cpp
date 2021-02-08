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

#include <system/epoc.h>
#include <kernel/kernel.h>
#include <kernel/process.h>

namespace scripting = eka2l1::scripting;
namespace py = pybind11;

namespace eka2l1::scripting {
    process::process(std::uint64_t handle)
        : process_handle(reinterpret_cast<eka2l1::kernel::process *>(handle)) {
    }

    py::bytes process::read_process_memory(const std::uint32_t addr, const size_t size) {
        void *ptr = process_handle->get_ptr_on_addr_space(addr);

        if (!ptr) {
            throw std::runtime_error("The memory at specified address hasn't been mapped yet!");
        }

        std::string buffer;

        buffer.resize(size);
        memcpy(&buffer[0], ptr, size);

        return buffer;
    }

    void process::write_process_memory(const std::uint32_t addr, const std::string &buffer) {
        void *ptr = process_handle->get_ptr_on_addr_space(addr);

        if (!ptr) {
            throw std::runtime_error("The memory at specified address hasn't been mapped yet!");
        }

        memcpy(ptr, buffer.data(), buffer.size());
    }

#define READ_TEMPLATE(ret_type)                                                              \
    void *ptr = process_handle->get_ptr_on_addr_space(addr);                                 \
    if (!ptr) {                                                                              \
        throw std::runtime_error("The memory at specified address hasn't been mapped yet!"); \
    }                                                                                        \
    return *reinterpret_cast<ret_type *>(ptr)

    std::uint8_t process::read_byte(const std::uint32_t addr) {
        READ_TEMPLATE(std::uint8_t);
    }

    std::uint16_t process::read_word(const std::uint32_t addr) {
        READ_TEMPLATE(std::uint16_t);
    }

    std::uint32_t process::read_dword(const std::uint32_t addr) {
        READ_TEMPLATE(std::uint32_t);
    }

    std::uint64_t process::read_qword(const std::uint32_t addr) {
        READ_TEMPLATE(std::uint64_t);
    }

    std::string process::get_executable_path() {
        return common::ucs2_to_utf8(process_handle->get_exe_path());
    }

    std::string process::get_name() {
        return process_handle->name();
    }

    std::unique_ptr<scripting::thread> process::first_thread() {
        return std::make_unique<scripting::thread>(reinterpret_cast<std::uint64_t>(process_handle->get_primary_thread()));
    }

    std::vector<std::unique_ptr<scripting::process>> get_process_list() {
        system *sys = get_current_instance();
        std::vector<kernel_obj_unq_ptr> &processes = sys->get_kernel_system()->get_process_list();

        std::vector<std::unique_ptr<scripting::process>> script_processes;

        for (const auto &pr : processes) {
            script_processes.push_back(std::make_unique<scripting::process>((uint64_t)(pr.get())));
        }

        return script_processes;
    }

    std::unique_ptr<eka2l1::scripting::process> get_current_process() {
        if (get_current_instance()->get_kernel_system()->crr_process() == nullptr) {
            return nullptr;
        }

        return std::make_unique<scripting::process>(reinterpret_cast<std::uint64_t>(
            get_current_instance()->get_kernel_system()->crr_process()));
    }
}

extern "C" {
    EKA2L1_EXPORT std::int32_t symemu_queries_all_processes(eka2l1::scripting::process ***pr) {
        if (!pr) {
            return -1;
        }
        
        eka2l1::system *sys = eka2l1::scripting::get_current_instance();
        std::vector<eka2l1::kernel_obj_unq_ptr> &processes = sys->get_kernel_system()->get_process_list();

        if (processes.empty()) {
            return 0;
        }

        *pr = new eka2l1::scripting::process*[processes.size()];

        for (std::size_t i = 0; i < processes.size(); i++) {
            (*pr)[i] = new eka2l1::scripting::process(reinterpret_cast<std::uint64_t>(processes[i].get()));
        }

        return static_cast<std::int32_t>(processes.size());
    }

    EKA2L1_EXPORT eka2l1::scripting::process *symemu_get_current_process() {
        eka2l1::kernel::process *pr = eka2l1::scripting::get_current_instance()->get_kernel_system()->crr_process();
        if (!pr) {
            return nullptr;
        }

        return new eka2l1::scripting::process(reinterpret_cast<std::uint64_t>(pr));
    }

    EKA2L1_EXPORT std::uint8_t symemu_process_read_byte(eka2l1::scripting::process *pr, const std::uint32_t addr) {
        return pr->read_byte(addr);
    }

    EKA2L1_EXPORT std::uint16_t symemu_process_read_word(eka2l1::scripting::process *pr, const std::uint32_t addr) {
        return pr->read_word(addr);
    }

    EKA2L1_EXPORT std::uint32_t symemu_process_read_dword(eka2l1::scripting::process *pr, const std::uint32_t addr) {
        return pr->read_dword(addr);
    }

    EKA2L1_EXPORT std::uint64_t symemu_process_read_qword(eka2l1::scripting::process *pr, const std::uint32_t addr) {
        return pr->read_qword(addr);
    }

    EKA2L1_EXPORT eka2l1::scripting::thread *symemu_process_first_thread(eka2l1::scripting::process *pr) {
        return new eka2l1::scripting::thread(reinterpret_cast<std::uint64_t>(pr->get_process_handle()->get_primary_thread()));
    }
}