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

#include <algorithm>
#include <cpu/12l1r/core_state.h>

namespace eka2l1::arm::r12l1 {
    core_state::core_state()
        : cpsr_(0)
        , fpscr_(0)
        , wrwr_(0)
        , ticks_left_(0)
        , should_break_(1)
        , current_aid_(0)
        , exclusive_state_(0)
        , entries_(nullptr) {
        std::fill(gprs_, gprs_ + sizeof(gprs_) / sizeof(std::uint32_t), 0);
        std::fill(fprs_, fprs_ + sizeof(fprs_) / sizeof(std::uint32_t), 0);
    }
}