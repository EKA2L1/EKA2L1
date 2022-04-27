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

#pragma once

#include <cpu/12l1r/common.h>
#include <cstdint>

namespace eka2l1::arm::r12l1 {
    struct tlb_entry;

    struct core_state {
        std::uint32_t gprs_[16];
        std::uint32_t cpsr_;
        std::uint32_t fprs_[64];
        std::uint32_t fpscr_;
        std::uint32_t wrwr_;

        std::int32_t ticks_left_;
        std::uint32_t should_break_;
        std::uint32_t current_aid_;
        std::uint32_t exclusive_state_;
        std::uint32_t fpscr_host_;

        tlb_entry *entries_;
        std::uint32_t padding_;

        explicit core_state();
    };

    static_assert(sizeof(core_state) % 8 == 0);
}