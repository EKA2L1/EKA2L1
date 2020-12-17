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

#include <cpu/12l1r/visit_session.h>
#include <cpu/12l1r/block_gen.h>

#include <common/bytes.h>

namespace eka2l1::arm::r12l1 {
    visit_session::visit_session(dashixiong_block *bro, translated_block *crr)
        : big_block_(bro)
        , crr_block_(crr)
        , reg_supplier_(bro) {
    }
    
    void visit_session::set_cond(common::cc_flags cc) {
        big_block_->set_cc(cc);
    }
    
    common::armgen::arm_reg visit_session::emit_address_lookup(common::armgen::arm_reg base) {
        return common::armgen::INVALID_REG;
    }

    bool visit_session::emit_memory_access_chain(common::armgen::arm_reg base, reg_list guest_list, bool add,
        bool before, bool writeback, bool load) {
        std::uint8_t last_reg = 0;

        base = reg_supplier_.map(base, 0);
        base = emit_address_lookup(base);

        reg_list host_reg_list = 0;
        reg_supplier_.spill_lock_all(REG_SCRATCH_TYPE_GPR);

        // Map to register as much registers as possible, then flush them all to load/store
        while (last_reg <= 15) {
            host_reg_list = 0;

            bool full = false;

            while (last_reg <= 15) {
                if (common::bit(guest_list, last_reg)) {
                    const common::armgen::arm_reg orig = static_cast<common::armgen::arm_reg>(
                        common::armgen::R0 + last_reg);

                    const common::armgen::arm_reg mapped = reg_supplier_.map(orig, 0);

                    if (mapped == common::armgen::INVALID_REG) {
                        full = true;
                        break;
                    } else {
                        host_reg_list |= (1 << (mapped - common::armgen::R0));
                    }
                }

                last_reg++;
            }

            if (load)
                big_block_->LDMBitmask(base, add, before, writeback, host_reg_list);
            else
                big_block_->STMBitmask(base, add, before, writeback, host_reg_list);

            if (full)
                reg_supplier_.flush_all();
        }
        
        reg_supplier_.release_spill_lock_all(REG_SCRATCH_TYPE_GPR);
        return true;
    }
}