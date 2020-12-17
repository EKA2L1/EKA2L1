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

namespace eka2l1::arm::r12l1 {
    struct core_state;
    class reg_cache;

    class dashixiong_block: public common::armgen::armx_codeblock {
    private:
        block_cache cache_;
        const void *dispatch_func_;

        memory_operation_32bit_func code_read_;

    protected:
        void assemble_control_funcs();

        translated_block *start_new_block(const vaddress addr, const asid aid);
        bool finalize_block(translated_block *block, const std::uint32_t guest_size);
        
        void emit_cycles_count_add(const std::uint32_t num);
        void emit_pc_flush(const address current_pc);

    public:
        explicit dashixiong_block();

        void enter_dispatch(core_state *cstate);

        translated_block *compile_new_block(core_state *state, const vaddress addr);
        translated_block *get_block(const vaddress addr, const asid aid);
        void emit_block_finalize(translated_block *block);

        void flush_range(const vaddress start, const vaddress end, const asid aid);
        void flush_all();
    };
}