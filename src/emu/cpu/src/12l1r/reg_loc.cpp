/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <cpu/12l1r/reg_cache.h>

namespace eka2l1::arm::r12l1 {
    guest_register_info::guest_register_info()
        : curr_location_(GUEST_REGISTER_LOC_MEM)
        , imm_(0)
        , spill_lock_(false)
        , last_use_(0) {

    }

    host_register_info::host_register_info()
        : guest_mapped_reg_(common::armgen::INVALID_REG)
        , scratch_(false)
        , dirty_(false) {
    }
}