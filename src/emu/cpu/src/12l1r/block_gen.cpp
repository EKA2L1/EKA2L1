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

#include <cpu/12l1r/block_gen.h>
#include <cpu/12l1r/reg_loc.h>
#include <cpu/12l1r/core_state.h>

#include <common/log.h>
#include <common/algorithm.h>

namespace eka2l1::arm::r12l1 {
    static constexpr std::size_t MAX_CODE_SPACE_BYTES = common::MB(24);

    static translated_block *dashixiong_get_block_proxy(dashixiong_block *self, const vaddress addr, const asid aid) {
        return self->get_block(addr, aid);
    }

    static translated_block *dashixiong_compile_new_block_proxy(dashixiong_block *self, core_state *state, const vaddress addr) {
        return self->compile_new_block(state, addr);
    }

    dashixiong_block::dashixiong_block()
        : dispatch_func_(nullptr) {
        alloc_codespace(MAX_CODE_SPACE_BYTES);
    }

    void dashixiong_block::assemble_control_funcs() {
        begin_write();
        dispatch_func_ = get_code_pointer();

        // Load the arguments to call lookup
        // While R0 is already the core state
        MOV(CORE_STATE_REG, common::armgen::R0);

        LDR(common::armgen::R1, common::armgen::R0, offsetof(core_state, gprs_[15]));
        LDRB(common::armgen::R2, common::armgen::R0, offsetof(core_state, current_aid_));
        MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(this));

        quick_call_function(common::armgen::R12, reinterpret_cast<void*>(dashixiong_get_block_proxy));
        
        CMPI2R(common::armgen::R0, 0, common::armgen::R12);
        common::armgen::fixup_branch available = B_CC(common::CC_NEQ);

        // Call for recompile
        MOV(common::armgen::R0, CORE_STATE_REG);
        LDR(common::armgen::R1, common::armgen::R0, offsetof(core_state, gprs_[15]));

        quick_call_function(common::armgen::R12, reinterpret_cast<void*>(dashixiong_compile_new_block_proxy));
        CMPI2R(common::armgen::R0, 0, common::armgen::R12);

        common::armgen::fixup_branch available_again = B_CC(common::CC_NEQ);
        
        // first set the jit state to break. 2 indicates error happened.
        // then exit (TODO)
        MOVI2R(common::armgen::R0, 2);
        STR(common::armgen::R0, CORE_STATE_REG, offsetof(core_state, should_break_));
        common::armgen::fixup_branch headout = B();

        set_jump_target(available);
        set_jump_target(available_again);

        LDR(common::armgen::R0, common::armgen::R0, offsetof(translated_block, translated_code_));
        BL(common::armgen::R0);                                                // Branch to the block

        set_jump_target(headout);
        end_write();
    }

    translated_block *dashixiong_block::start_new_block(const vaddress addr, const asid aid) {
        bool try_new_block_result = cache_.add_block(addr, aid);
        if (!try_new_block_result) {
            LOG_ERROR(CPU_12L1R, "Trying to start a block that already exists!");
            return nullptr;
        }

        translated_block *blck = cache_.lookup_block(addr, aid);
        blck->translated_code_ = get_code_pointer();

        return blck;
    }

    bool dashixiong_block::finalize_block(translated_block *block, const std::uint32_t guest_size) {
        if (!block->translated_code_) {
            LOG_ERROR(CPU_12L1R, "This block is invalid (addr = 0x{:X})!", block->start_address());
            return false;
        }

        block->size_ = guest_size;
        block->translated_size_ = get_code_pointer() - block->translated_code_;

        return true;
    }

    translated_block *dashixiong_block::get_block(const vaddress addr, const asid aid) {
        return cache_.lookup_block(addr, aid);
    }

    void dashixiong_block::flush_range(const vaddress start, const vaddress end, const asid aid) {
        cache_.flush_range(start, end, aid);
    }

    void dashixiong_block::flush_all() {
        cache_.flush_all();
    }

    void dashixiong_block::enter_dispatch(core_state *cstate) {
        typedef void (*dispatch_func_type)(core_state *state);
        dispatch_func_type df = reinterpret_cast<dispatch_func_type>(dispatch_func_);

        df(cstate);
    }

    translated_block *dashixiong_block::compile_new_block(core_state *state, const vaddress addr) {
        return nullptr;
    }
}