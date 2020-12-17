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

#include <cpu/12l1r/reg_cache.h>
#include <cstdint>

namespace eka2l1::arm::r12l1 {
    class dashixiong_block;
    struct translated_block;

    class visit_session {
    public:
        translated_block *crr_block_;
        dashixiong_block *big_block_;

        reg_cache reg_supplier_;

        explicit visit_session(dashixiong_block *bro, translated_block *crr);
        void set_cond(common::cc_flags cc);
        
        common::armgen::arm_reg emit_address_lookup(common::armgen::arm_reg base);
        bool emit_memory_access_chain(common::armgen::arm_reg base, reg_list guest_list, bool add,
            bool before, bool writeback, bool load);
    };
}