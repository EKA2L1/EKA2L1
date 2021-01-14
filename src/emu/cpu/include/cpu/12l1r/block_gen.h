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

#include <common/armemitter.h>
#include <cpu/12l1r/block_cache.h>
#include <cpu/arm_interface.h>

#include <cstdint>
#include <map>

namespace eka2l1::arm::r12l1 {
    struct core_state;
    class reg_cache;
	class visit_session;

    struct dashixiong_callback {
        memory_operation_8bit_func &read_byte_;
        memory_operation_16bit_func &read_word_;
        memory_operation_32bit_func &read_dword_;
        memory_operation_64bit_func &read_qword_;
		
        memory_operation_8bit_func &write_byte_;
        memory_operation_16bit_func &write_word_;
        memory_operation_32bit_func &write_dword_;
        memory_operation_64bit_func &write_qword_;
		
        memory_operation_32bit_func &code_read_;

        handle_exception_func &exception_handler_;
        system_call_handler_func &syscall_handler_;
    };

    class dashixiong_block: public common::armgen::armx_codeblock {
    private:
        std::multimap<translated_block::hash_type, translated_block*> link_to_;

        block_cache cache_;

        void *dispatch_func_;
        const void *dispatch_ent_for_block_;

        dashixiong_callback callbacks_;

    protected:
        void assemble_control_funcs();

        translated_block *start_new_block(const vaddress addr, const asid aid);

    public:
        explicit dashixiong_block(dashixiong_callback &callbacks);

        void enter_dispatch(core_state *cstate);

        translated_block *compile_new_block(core_state *state, const vaddress addr);
        translated_block *get_block(const vaddress addr, const asid aid);

        void emit_block_links(translated_block* block);
        void emit_return_to_dispatch(translated_block *block);
		void edit_block_links(translated_block *dest, bool unlink = false);

        void emit_pc_flush(const address current_pc);
        void emit_pc_write_exchange(common::armgen::arm_reg pc_reg);
        void emit_pc_write(common::armgen::arm_reg pc_reg);
		void emit_cycles_count_add(const std::uint32_t num);
        void emit_cpsr_save();
        void emit_cycles_count_save();

        void flush_range(const vaddress start, const vaddress end, const asid aid);
        void flush_all();

        void raise_guest_exception(const exception_type exc, const std::uint32_t usrdata);
        void raise_system_call(const std::uint32_t num);
		
		std::uint8_t read_byte(const vaddress addr);
		std::uint16_t read_word(const vaddress addr);
		std::uint32_t read_dword(const vaddress addr);
		std::uint64_t read_qword(const vaddress addr);
		
		void write_byte(const vaddress addr, std::uint8_t dat);
		void write_word(const vaddress addr, std::uint16_t dat);
		void write_dword(const vaddress addr, std::uint32_t dat);
		void write_qword(const vaddress addr, std::uint64_t dat);
    };
}