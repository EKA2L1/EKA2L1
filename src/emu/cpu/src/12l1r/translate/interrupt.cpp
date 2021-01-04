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
    bool arm_translate_visitor::arm_SVC(common::cc_flags cond, const std::uint32_t n) {
        set_cond(cond);
        return emit_system_call_handler(n);
    }

    bool arm_translate_visitor::arm_UDF() {
        set_cond(common::CC_AL);
        return emit_undefined_instruction_handler();
    }
}