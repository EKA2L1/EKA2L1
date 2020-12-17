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

namespace eka2l1::arm::r12l1 {
    class dashixiong_block;
    class reg_cache;

    class arm_translate_visitor {
    private:
        dashixiong_block *big_block_;
        translated_block *target_block_;

        reg_cache *reg_supplier_;

    public:
        explicit arm_translate_visitor(dashixiong_block *bblock, reg_cache *supplier);
        void set_target_block(translated_block *block);

        bool arm_LDM(common::cc_flags cond, bool W, common::armgen::reg n, reg_list list);
        bool arm_LDMDA(common::cc_flags cond, bool W, common::armgen::reg n, reg_list list);
        bool arm_LDMDB(common::cc_flags cond, bool W, common::armgen::reg n, reg_list list);
        bool arm_LDMIB(common::cc_flags cond, bool W, common::armgen::reg n, reg_list list);
        bool arm_STM(common::cc_flags cond, bool W, Reg n, reg_list list);
        bool arm_STMDA(common::cc_flags cond, bool W, Reg n, reg_list list);
        bool arm_STMDB(common::cc_flags cond, bool W, Reg n, reg_list list);
        bool arm_STMIB(common::cc_flags cond, bool W, Reg n, reg_list list);
    };
}