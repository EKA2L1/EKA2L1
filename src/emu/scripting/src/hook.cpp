/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

#include <system/epoc.h>


#include <scripting/manager.h>

namespace eka2l1::scripting {
    void register_lib_invokement(const std::string &lib_name, const std::uint32_t ord, const std::uint32_t process_uid, pybind11::function func) {
        get_current_instance()->get_scripts()->register_library_hook(lib_name, ord, process_uid, func);
    }

    void register_breakpoint_invokement(const std::string &image_name, const uint32_t addr, const std::uint32_t process_uid, pybind11::function func) {
        get_current_instance()->get_scripts()->register_breakpoint(image_name, addr, process_uid, func);
    }

    void register_ipc_invokement(const std::string &server_name, const int opcode, const int when, pybind11::function func) {
        get_current_instance()->get_scripts()->register_ipc(server_name, opcode, when, func);
    }
}

extern "C" {
    EKA2L1_EXPORT void symemu_free_string(char *pt) {
        delete pt;
    }

    EKA2L1_EXPORT void symemu_cpu_register_lib_hook(const char *lib_name, const std::uint32_t ord, const std::uint32_t process_uid, eka2l1::manager::breakpoint_hit_lua_func func) {
        eka2l1::scripting::get_current_instance()->get_scripts()->register_library_hook(lib_name, ord, process_uid, func);
    }

    EKA2L1_EXPORT void symemu_cpu_register_bkpt_hook(const char *image_name, const std::uint32_t addr, const std::uint32_t process_uid, eka2l1::manager::breakpoint_hit_lua_func func) {
        eka2l1::scripting::get_current_instance()->get_scripts()->register_breakpoint(image_name, addr, process_uid, func);
    }

    EKA2L1_EXPORT void symemu_register_ipc_sent_hook(const char *server_name, const int opcode, eka2l1::manager::ipc_sent_lua_func func) {
        eka2l1::scripting::get_current_instance()->get_scripts()->register_ipc(server_name, opcode, 0, reinterpret_cast<void*>(func));
    }
    
    EKA2L1_EXPORT void symemu_register_ipc_completed_hook(const char *server_name, const int opcode, eka2l1::manager::ipc_completed_lua_func func) {
        eka2l1::scripting::get_current_instance()->get_scripts()->register_ipc(server_name, opcode, 2, reinterpret_cast<void*>(func));
    }
}