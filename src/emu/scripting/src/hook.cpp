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

#include <scripting/instance.h>
#include <scripting/platform.h>

#include <scripting/manager.h>
#include <system/epoc.h>

#include <common/cvt.h>
#include <cstring>

extern "C" {
EKA2L1_EXPORT void eka2l1_free_string(char *pt) {
    delete pt;
}

EKA2L1_EXPORT const char *eka2l1_std_utf16_to_utf8(const char *wstring, const int length) {
    std::u16string data(reinterpret_cast<const char16_t *>(wstring), length);
    std::string result = eka2l1::common::ucs2_to_utf8(data);

    char *result_raw = new char[result.length() + 1];

    std::memcpy(result_raw, result.data(), length);
    result[result.length()] = '\0';

    return result_raw;
}

EKA2L1_EXPORT std::uint32_t eka2l1_cpu_register_lib_hook(const char *lib_name, const std::uint32_t ord, const std::uint32_t process_uid, const std::uint32_t uid3, const std::uint32_t seghash, eka2l1::manager::breakpoint_hit_func func) {
    return eka2l1::scripting::get_current_instance()->get_scripts()->register_library_hook(lib_name, ord, process_uid, uid3, seghash, func);
}

EKA2L1_EXPORT std::uint32_t eka2l1_cpu_register_bkpt_hook(const char *image_name, const std::uint32_t addr, const std::uint32_t process_uid, const std::uint32_t uid3, const std::uint32_t seghash, eka2l1::manager::breakpoint_hit_func func) {
    return eka2l1::scripting::get_current_instance()->get_scripts()->register_breakpoint(image_name, addr, process_uid, uid3, seghash, func);
}

EKA2L1_EXPORT std::uint32_t eka2l1_register_ipc_sent_hook(const char *server_name, const int opcode, eka2l1::manager::ipc_sent_func func) {
    return eka2l1::scripting::get_current_instance()->get_scripts()->register_ipc(server_name, opcode, 0, reinterpret_cast<void *>(func));
}

EKA2L1_EXPORT std::uint32_t eka2l1_register_ipc_completed_hook(const char *server_name, const int opcode, eka2l1::manager::ipc_completed_func func) {
    return eka2l1::scripting::get_current_instance()->get_scripts()->register_ipc(server_name, opcode, 2, reinterpret_cast<void *>(func));
}

EKA2L1_EXPORT void eka2l1_clear_hook(const std::uint32_t handle) {
    eka2l1::scripting::get_current_instance()->get_scripts()->remove_function(handle);
}
}
