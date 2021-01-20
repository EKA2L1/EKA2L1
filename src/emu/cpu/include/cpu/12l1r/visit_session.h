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
        common::cc_flags flag_;
        common::armgen::fixup_branch end_target_;

		bool cond_modified_;
		bool cpsr_ever_updated_;

		bool cond_failed_;

		std::uint32_t last_inst_count_;

    public:
        translated_block *crr_block_;
        dashixiong_block *big_block_;

        reg_cache reg_supplier_;

        explicit visit_session(dashixiong_block *bro, translated_block *crr);
        bool condition_passed(common::cc_flags cc, const bool force_end_last = false);
		
        common::armgen::arm_reg emit_address_lookup(common::armgen::arm_reg base, const bool for_read,
        	std::uint8_t **lookup_route = nullptr);

        bool emit_memory_access_chain(common::armgen::arm_reg base, reg_list guest_list, bool add,
            bool before, bool writeback, bool load);

        bool emit_memory_access(common::armgen::arm_reg target, common::armgen::arm_reg base,
        	common::armgen::operand2 op2, const std::uint8_t bit_count, bool is_signed, bool add, bool pre_index, bool writeback, bool read);

        bool emit_undefined_instruction_handler();
        bool emit_unimplemented_behaviour_handler();
        bool emit_system_call_handler(const std::uint32_t n);

		void emit_cpsr_update_nzcvq();
		void emit_cpsr_restore_nzcvq();
		void emit_cpsr_update_sel(const bool nzcvq = true);
		void emit_cpsr_restore_sel(const bool nzcvq = true);

		void emit_direct_link(const vaddress addr, const bool save_cpsr = false);
		void emit_return_to_dispatch(const bool save_cpsr = true);
		void emit_pc_write_exchange(common::armgen::arm_reg reg);
		void emit_pc_write(common::armgen::arm_reg reg);
		void emit_alu_jump(common::armgen::arm_reg reg);

		void sync_state();
		void finalize();

		void cpsr_nzcvq_changed() {
			cond_modified_ = true;
			cpsr_ever_updated_ = true;
		}

		void cpsr_ge_changed() {
			cpsr_ever_updated_ = true;
		}
    };
}