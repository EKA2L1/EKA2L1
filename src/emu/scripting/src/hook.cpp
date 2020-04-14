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

#include <scripting/hook.h>
#include <scripting/instance.h>

#include <epoc/epoc.h>

#include <manager/manager.h>
#include <manager/script_manager.h>

namespace eka2l1::scripting {
    void register_panic_invokement(const std::string &category, pybind11::function ifunc) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_panic(category, ifunc);
    }

    void register_svc_invokement(int svc_num, int time, pybind11::function ifunc) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_svc(svc_num, time, ifunc);
    }

    void register_reschedule_invokement(pybind11::function ifunc) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_reschedule(ifunc);
    }

    void register_lib_invokement(const std::string &lib_name, const std::uint32_t ord, pybind11::function func) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_library_hook(lib_name, ord, func);
    }

    void register_breakpoint_invokement(const uint32_t addr, pybind11::function func) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_breakpoint(addr, func);
    }

    void register_ipc_invokement(const std::string &server_name, const int opcode, const int when, pybind11::function func) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_ipc(server_name, opcode, when, func);
    }
}
