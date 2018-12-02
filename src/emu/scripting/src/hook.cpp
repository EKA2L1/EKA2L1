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

#include <core/core.h>
#include <core/manager/script_manager.h>

namespace eka2l1::scripting {
    void register_panic_invokement(const std::string &category, pybind11::function ifunc) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_panic(category, ifunc);
    }

    void register_svc_invokement(int svc_num, pybind11::function ifunc) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_svc(svc_num, ifunc);
    }

    void register_reschedule_invokement(pybind11::function ifunc) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_reschedule(ifunc);
    }

    void register_sid_invokement(const uint32_t sid, pybind11::function func) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_sid(sid, func);
    }

    void register_breakpoint_invokement(const uint32_t addr, pybind11::function func) {
        get_current_instance()->get_manager_system()->get_script_manager()->register_breakpoint(addr, func);
    }
}