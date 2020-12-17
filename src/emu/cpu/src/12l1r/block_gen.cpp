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

#include <cpu/12l1r/encoding/arm.h>
#include <cpu/12l1r/encoding/thumb16.h>
#include <cpu/12l1r/visit_session.h>
#include <cpu/12l1r/arm_visitor.h>

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
        LDR(common::armgen::R2, common::armgen::R0, offsetof(core_state, current_aid_));
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

    void dashixiong_block::emit_cycles_count_add(const std::uint32_t num) {
        LDR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, ticks_left_));
        SUBI2R(ALWAYS_SCRATCH1, ALWAYS_SCRATCH1, num, ALWAYS_SCRATCH2);
        STR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, ticks_left_));

        // Also if we are having times check if we are out of cycles
        CMP(ALWAYS_SCRATCH1, 0);
        
        set_cc(common::CC_LE);
        
        {
            MOVI2R(ALWAYS_SCRATCH1, 1);
            STR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, should_break_));
        }

        set_cc(common::CC_AL);
    }

    void dashixiong_block::emit_pc_flush(const address current_pc) {
        MOVI2R(ALWAYS_SCRATCH1, current_pc);
        STR(ALWAYS_SCRATCH2, CORE_STATE_REG, offsetof(core_state, gprs_[15]));
    }
    
    void dashixiong_block::emit_block_finalize(translated_block *block) {
        emit_cycles_count_add(block->inst_count_);
        emit_pc_flush(block->current_address());
    }

    translated_block *dashixiong_block::compile_new_block(core_state *state, const vaddress addr) {
        translated_block *block = start_new_block(addr, state->current_aid_);
        if (!block) {
            return nullptr;
        }

        const bool is_thumb = (state->cpsr_ & CPSR_THUMB_FLAG_MASK);
        bool should_quit = false;

        visit_session context(this, block);
        std::unique_ptr<arm_translate_visitor> arm_visitor = nullptr;

        if (!is_thumb) {
            arm_visitor = std::make_unique<arm_translate_visitor>(&context);
        }

        // Emit the check if we should go outside and stop running
        LDR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, should_break_));
        CMP(ALWAYS_SCRATCH1, 0);

        set_cc(common::CC_NEQ);

        {
            // Branch to dispatch if we should quit...
            MOV(common::armgen::R0, CORE_STATE_REG);
            B_CC(common::CC_NEQ, dispatch_func_);
        }

        set_cc(common::CC_AL);

        do {
            std::uint32_t inst = 0;
            if (!code_read_(addr, &inst)) {
                LOG_ERROR(CPU_12L1R, "Error while reading instruction at address 0x{:X}!, addr");
                return nullptr;
            }

            block->size_ += (is_thumb) ? 2 : 4;
            block->inst_count_++;

            if (is_thumb) {
                LOG_ERROR(CPU_12L1R, "Missing thumb handler!");
                //auto launch = decode_thumb16(static_cast<std::uint16_t>(inst));
            } else {
                // ARM decoding. NEON -> VFP -> ARM
                if (auto decoder = decode_arm<arm_translate_visitor>(inst)) {
                    should_quit = decoder->get().call(*arm_visitor, inst);
                }
            }
        } while (true);

        // Emit cycles count add
        if (!finalize_block(block, block->size_)) {
            LOG_WARN(CPU_12L1R, "Unable to finalzie block!");
        }

        return block;
    }
}