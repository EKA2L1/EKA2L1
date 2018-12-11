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

#include <hle/return_val.h>

namespace eka2l1 {
    namespace hle {
        template <>
        void write_return_value(arm::jitter &cpu, uint16_t ret) {
            cpu->set_reg(0, ret);
        }

        template <>
        void write_return_value(arm::jitter &cpu, int32_t ret) {
            cpu->set_reg(0, ret);
        }

        template <>
        void write_return_value(arm::jitter &cpu, uint32_t ret) {
            cpu->set_reg(0, ret);
        }

        template <>
        void write_return_value(arm::jitter &cpu, bool ret) {
            cpu->set_reg(0, ret);
        }

        template <>
        uint16_t read_return_value(arm::jitter &cpu) {
            return cpu->get_reg(0);
        }

        template <>
        uint32_t read_return_value(arm::jitter &cpu) {
            return cpu->get_reg(0);
        }

        template <>
        int32_t read_return_value(arm::jitter &cpu) {
            return cpu->get_reg(0);
        }

        template <>
        bool read_return_value(arm::jitter &cpu) {
            return cpu->get_reg(0);
        }
    }
}