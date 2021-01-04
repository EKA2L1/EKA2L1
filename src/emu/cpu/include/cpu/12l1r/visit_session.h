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
#include <vector>

namespace eka2l1::arm::r12l1 {
    class dashixiong_block;
    struct dashixiong_callback;

    struct translated_block;

    inline common::armgen::arm_reg reg_index_to_gpr(const reg_index idx) {
        return static_cast<common::armgen::arm_reg>(common::armgen::R0 + idx);
    }

    void dashixiong_print_22debug(const std::uint32_t val);

    class visit_session {
	protected:
        std::vector<common::armgen::fixup_branch> ret_to_dispatch_branches_;
        common::armgen::fixup_branch end_of_cond_;

        common::cc_flags last_flag_;

		bool cpsr_modified_;				///< Has the CPSR been modified since last time the flag is updated.
		bool cpsr_ever_updated_;			///< Has the CPSR ever been updated during the translation.
		bool is_cond_block_;

    public:
        translated_block *crr_block_;
        dashixiong_block *big_block_;

        reg_cache reg_supplier_;

        explicit visit_session(dashixiong_block *bro, translated_block *crr);
        void set_cond(common::cc_flags cc, const bool cpsr_will_ruin = false);
		
        common::armgen::arm_reg emit_address_lookup(common::armgen::arm_reg base, const bool for_read);

        bool emit_memory_access_chain(common::armgen::arm_reg base, reg_list guest_list, bool add,
            bool before, bool writeback, bool load);

        bool emit_memory_access(common::armgen::arm_reg target_mapped, common::armgen::arm_reg base_mapped,
        	common::armgen::operand2 op2, const std::uint8_t bit_count, bool is_signed, bool add, bool pre_index, bool writeback, bool read);

        bool emit_undefined_instruction_handler();
        bool emit_system_call_handler(const std::uint32_t n);
		
		void emit_cpsr_update_nzcv();
		void emit_cpsr_restore_nzcv();

		void emit_direct_link(const vaddress addr);
		void emit_reg_link_exchange(common::armgen::arm_reg reg);
		void emit_return_to_dispatch();

		void sync_registers();

		void finalize();

		void cpsr_nzcv_changed() {
			cpsr_modified_ = true;
			cpsr_ever_updated_ = true;
		}
    };
}