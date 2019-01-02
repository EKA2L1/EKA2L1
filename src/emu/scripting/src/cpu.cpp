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

#include <scripting/cpu.h>
#include <scripting/instance.h>

#include <arm/arm_interface.h>
#include <epoc/epoc.h>

namespace eka2l1::scripting {
    uint32_t cpu::get_register(const int index) {
        return get_current_instance()->get_cpu()->get_reg(index);
    }

    uint32_t cpu::get_cpsr() {
        return get_current_instance()->get_cpu()->get_cpsr();
    }

    uint32_t cpu::get_pc() {
        return get_current_instance()->get_cpu()->get_pc();
    }

    uint32_t cpu::get_sp() {
        return get_current_instance()->get_cpu()->get_sp();
    }

    uint32_t cpu::get_lr() {
        return get_current_instance()->get_cpu()->get_lr();
    }
}