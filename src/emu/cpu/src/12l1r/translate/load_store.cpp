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

#include <cpu/12l1r/arm_visitor.h>
#include <cpu/12l1r/visit_session.h>

namespace eka2l1::arm::r12l1 {
    bool arm_translate_visitor::arm_LDM(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        session_->set_cond(cond);
        return session_->emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, true, false, W, true);
    }

    bool arm_translate_visitor::arm_LDMDA(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        session_->set_cond(cond);
        return session_->emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, false, false, W, true);
    }

    bool arm_translate_visitor::arm_LDMDB(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        session_->set_cond(cond);
        return session_->emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, false, true, W, true);
    }

    bool arm_translate_visitor::arm_LDMIB(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        session_->set_cond(cond);
        return session_->emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, true, true, W, true);
    }

    bool arm_translate_visitor::arm_STM(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        session_->set_cond(cond);
        return session_->emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, true, false, W, false);
    }

    bool arm_translate_visitor::arm_STMDA(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        session_->set_cond(cond);
        return session_->emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, false, false, W, false);
    }
    
    bool arm_translate_visitor::arm_STMDB(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        session_->set_cond(cond);
        return session_->emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, false, true, W, false);
    }

    bool arm_translate_visitor::arm_STMIB(common::cc_flags cond, bool W, reg_index n, reg_list list) {
        session_->set_cond(cond);
        return session_->emit_memory_access_chain(static_cast<common::armgen::arm_reg>(common::armgen::R0 + n), list, true, true, W, false);
    }
}