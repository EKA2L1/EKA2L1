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

#include <cstdint>

#include <common/algorithm.h>
#include <common/log.h>
#include <cpu/arm_utils.h>

namespace eka2l1::arm {
    void dump_context(const core::thread_context &uni) {
        LOG_TRACE(CPU, "CPU context: ");
        LOG_TRACE(CPU, "pc: 0x{:x}", uni.get_pc());
        LOG_TRACE(CPU, "lr: 0x{:x}", uni.get_lr());
        LOG_TRACE(CPU, "sp: 0x{:x}", uni.get_sp());
        LOG_TRACE(CPU, "cpsr: 0x{:x}", uni.cpsr);

        for (std::size_t i = 0; i < uni.cpu_registers.size(); i++) {
            LOG_TRACE(CPU, "r{}: 0x{:x}", i, uni.cpu_registers[i]);
        }
    }

    const char *arm_emulator_type_to_string(const arm_emulator_type type) {
        switch (type) {
        case arm_emulator_type::dynarmic:
            return dynarmic_jit_backend_formal_name;

        case arm_emulator_type::unicorn:
            return unicorn_jit_backend_formal_name;

        case arm_emulator_type::dyncom:
            return dyncom_jit_backend_formal_name;

        case arm_emulator_type::r12l1:
            return r12l1_jit_backend_formal_name;

        default:
            break;
        }

        return "Unknown";
    }

    arm_emulator_type string_to_arm_emulator_type(const std::string &name) {
        const std::string backend_lowered = common::lowercase_string(name);

        if (backend_lowered == unicorn_jit_backend_name)
            return arm_emulator_type::unicorn;

        if (backend_lowered == dynarmic_jit_backend_name)
            return arm_emulator_type::dynarmic;

        if (backend_lowered == dyncom_jit_backend_name) {
            return arm_emulator_type::dyncom;
        }

        if (backend_lowered == r12l1_jit_backend_name) {
            return arm_emulator_type::r12l1;
        }

        return arm_emulator_type::dynarmic;
    }
}