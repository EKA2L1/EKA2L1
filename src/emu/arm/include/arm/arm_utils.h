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

#pragma once

#include <arm/arm_interface.h>
#include <string>

namespace eka2l1::arm {
    // Add JIT backend name here
    static constexpr const char *dynarmic_jit_backend_name = "dynarmic"; ///< Dynarmic recompiler backend name
    static constexpr const char *unicorn_jit_backend_name = "unicorn"; ///< Unicorn recompiler backend name
    static constexpr const char *earm_jit_backend_name = "earm"; ///< EKA2L1's ARM recompiler backend name

    static constexpr const char *dynarmic_jit_backend_formal_name = "Dynarmic"; ///< Dynarmic recompiler backend name
    static constexpr const char *unicorn_jit_backend_formal_name = "Unicorn"; ///< Unicorn recompiler backend name
    static constexpr const char *earm_jit_backend_formal_name = "EARM"; ///< EKA2L1's ARM recompiler backend name

    /**
     * \brief Dump the given thread context to log.
     * \param uni   The thread context.
     * 
     * \internal
     */
    void dump_context(const arm_interface::thread_context &uni);

    /**
     * \brief       Translate ARM emulator type variable to string.
     * \param       type      The ARM emulator type.
     * \returns     "Unknown" on unknown emulator type. Else the string to the name.
     */
    const char *arm_emulator_type_to_string(const arm_emulator_type type);

    /**
     * \brief       Translate string to ARM emulator type.
     * 
     * If the string does not match anything we have, a default backend is used.
     * 
     * \param       name        The string to translate.
     * \returns     A backend correspond to the name.
     */
    arm_emulator_type string_to_arm_emulator_type(const std::string &name);
}