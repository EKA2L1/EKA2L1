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
#include <cpu/12l1r/encoding/thumb32.h>
#include <cpu/12l1r/visit_session.h>
#include <cpu/12l1r/arm_visitor.h>
#include <cpu/12l1r/thumb_visitor.h>

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

    dashixiong_block::dashixiong_block(dashixiong_callback &callbacks)
        : dispatch_func_(nullptr)
        , dispatch_ent_for_block_(nullptr)
        , callbacks_(std::move(callbacks)) {
        context_info.detect();

        alloc_codespace(MAX_CODE_SPACE_BYTES);
        assemble_control_funcs();

        cache_.set_on_block_invalidate_callback([this](translated_block *to_destroy) {
            edit_block_links(to_destroy, true);
        });
    }

    static void dashixiong_print_debug(const std::uint32_t val) {
        LOG_TRACE(CPU_12L1R, "Print debug: 0x{:X}", val);
    }

    void dashixiong_block::assemble_control_funcs() {
        begin_write();
        dispatch_func_ = get_writeable_code_ptr();

        // Load the arguments to call lookup
        // While R0 is already the core state
        PUSH(9, common::armgen::R4, common::armgen::R5, common::armgen::R6, common::armgen::R7, common::armgen::R8,
                common::armgen::R9, common::armgen::R10, common::armgen::R12, common::armgen::R14);

        MOV(CORE_STATE_REG, common::armgen::R0);

        dispatch_ent_for_block_ = get_code_ptr();

        LDR(common::armgen::R1, CORE_STATE_REG, offsetof(core_state, should_break_));
        CMPI2R(common::armgen::R1, 0, common::armgen::R12);

        auto return_back = B_CC(common::CC_NEQ);

        LDR(common::armgen::R1, CORE_STATE_REG, offsetof(core_state, gprs_[15]));
        LDR(common::armgen::R2, CORE_STATE_REG, offsetof(core_state, current_aid_));
        MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(this));

        quick_call_function(common::armgen::R12, reinterpret_cast<void*>(dashixiong_get_block_proxy));

        CMPI2R(common::armgen::R0, 0, common::armgen::R12);
        common::armgen::fixup_branch available = B_CC(common::CC_NEQ);

        // Call for recompile
        MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(this));
        MOV(common::armgen::R1, CORE_STATE_REG);
        LDR(common::armgen::R2, CORE_STATE_REG, offsetof(core_state, gprs_[15]));

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
        B(common::armgen::R0);                                                // Branch to the block

        set_jump_target(headout);
        set_jump_target(return_back);

        // Branch back to where it went
        POP(9, common::armgen::R4, common::armgen::R5, common::armgen::R6, common::armgen::R7, common::armgen::R8,
             common::armgen::R9, common::armgen::R10, common::armgen::R12, common::armgen::R15);

        flush_lit_pool();
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

    void dashixiong_block::edit_block_links(translated_block *dest, bool unlink) {
        auto link_ites = link_to_.equal_range(dest->hash_);
        const auto psize = common::get_host_page_size();

        for (auto ite = link_ites.first; ite != link_ites.second; ite++) {
            translated_block *request_link_block = ite->second;

            for (auto &link: request_link_block->links_) {
                if (link.to_ == dest->start_address()) {
                    void *aligned_addr = common::align_address_to_host_page(link.value_);

                    if (common::is_memory_wx_exclusive()) {
                        common::change_protection(aligned_addr, psize, prot_read_write);
                    }

                    common::cpu_info temp_info = context_info;
                    common::armgen::armx_emitter temp_emitter(reinterpret_cast<std::uint8_t*>(link.value_),
                                                              temp_info);

                    if (unlink) {
                        emit_pc_flush(link.to_);
                        temp_emitter.B(dispatch_ent_for_block_);

                        link.linked_ = false;
                    } else {
                        temp_emitter.B(dest->translated_code_);
                        link.linked_ = true;
                    }

                    temp_emitter.flush_lit_pool();
                    temp_emitter.flush_icache();

                    if (common::is_memory_wx_exclusive()) {
                        common::change_protection(aligned_addr, psize, prot_read_exec);
                    }
                }
            }
        }
    }

    translated_block *dashixiong_block::get_block(const vaddress addr, const asid aid) {
        return cache_.lookup_block(addr, aid);
    }

    void dashixiong_block::flush_range(const vaddress start, const vaddress end, const asid aid) {
        cache_.flush_range(start, end, aid);
    }

    void dashixiong_block::flush_all() {
        link_to_.clear();
        cache_.flush_all();
    }

    void dashixiong_block::raise_guest_exception(const exception_type exc, const std::uint32_t usrdata) {
        callbacks_.exception_handler_(exc, usrdata);
    }

    void dashixiong_block::raise_system_call(const std::uint32_t num) {
        callbacks_.syscall_handler_(num);
    }

    std::uint8_t dashixiong_block::read_byte(const vaddress addr) {
        std::uint8_t value = 0;
        bool res = callbacks_.read_byte_(addr, &value);

        if (!res) {
            LOG_ERROR(CPU_12L1R, "Failed to read BYTE at address 0x{:X}", addr);
        }

        return value;
    }

    std::uint16_t dashixiong_block::read_word(const vaddress addr) {
        std::uint16_t value = 0;
        bool res = callbacks_.read_word_(addr, &value);

        if (!res) {
            LOG_ERROR(CPU_12L1R, "Failed to read WORD at address 0x{:X}", addr);
        }

        return value;
    }

    std::uint32_t dashixiong_block::read_dword(const vaddress addr) {
        std::uint32_t value = 0;
        bool res = callbacks_.read_dword_(addr, &value);

        if (!res) {
            LOG_ERROR(CPU_12L1R, "Failed to read DWORD at address 0x{:X}", addr);
        }

        return value;
    }

    std::uint64_t dashixiong_block::read_qword(const vaddress addr) {
        std::uint64_t value = 0;
        bool res = callbacks_.read_qword_(addr, &value);

        if (!res) {
            LOG_ERROR(CPU_12L1R, "Failed to read QWORD at address 0x{:X}", addr);
        }

        return value;
    }

    void dashixiong_block::write_byte(const vaddress addr, std::uint8_t dat) {
        if (!callbacks_.write_byte_(addr, &dat)) {
            LOG_ERROR(CPU_12L1R, "Failed to write BYTE to address 0x{:X}", addr);
        }
    }

    void dashixiong_block::write_word(const vaddress addr, std::uint16_t dat) {
        if (!callbacks_.write_word_(addr, &dat)) {
            LOG_ERROR(CPU_12L1R, "Failed to write WORD to address 0x{:X}", addr);
        }
    }

    void dashixiong_block::write_dword(const vaddress addr, std::uint32_t dat) {
        if (!callbacks_.write_dword_(addr, &dat)) {
            LOG_ERROR(CPU_12L1R, "Failed to write DWORD to address 0x{:X}", addr);
        }
    }

    void dashixiong_block::write_qword(const vaddress addr, std::uint64_t dat) {
        if (!callbacks_.write_qword_(addr, &dat)) {
            LOG_ERROR(CPU_12L1R, "Failed to write QWORD to address 0x{:X}", addr);
        }
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
    }

    void dashixiong_block::emit_pc_flush(const address current_pc) {
        MOVI2R(ALWAYS_SCRATCH1, current_pc);
        STR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, gprs_[15]));
    }

    void dashixiong_block::emit_cpsr_save() {
        STR(CPSR_REG, CORE_STATE_REG, offsetof(core_state, cpsr_));
    }

    void dashixiong_block::emit_pc_write_exchange(common::armgen::arm_reg pc_reg) {
        MOV(ALWAYS_SCRATCH1, pc_reg);
        BIC(CPSR_REG, CPSR_REG, CPSR_THUMB_FLAG_MASK);      // Clear T flag

        TST(ALWAYS_SCRATCH1, 1);

        set_cc(common::CC_NEQ);

        {
            // Well we switch to thumb mode, so set T flag
            ORI2R(CPSR_REG, CPSR_REG, 0x20, ALWAYS_SCRATCH1);
            BIC(ALWAYS_SCRATCH1, ALWAYS_SCRATCH1, 1);
        }

        set_cc(common::CC_AL);

        STR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, gprs_[15]));
    }

    void dashixiong_block::emit_block_links(translated_block* block) {
        for (auto &link: block->links_) {
            for (auto &jump_target: link.needs_) {
                set_jump_target(jump_target);
            }

            emit_cpsr_save();
            emit_cycles_count_add(block->inst_count_);

            link_to_.emplace(make_block_hash(link.to_, block->address_space()), block);
            link.value_ = reinterpret_cast<std::uint32_t*>(get_writeable_code_ptr());

            // Can we link the block now?
            if (auto link_block = get_block(link.to_, block->address_space())) {
                B(link_block->translated_code_);

                // Reserved two words for unlink?
                NOP();
                NOP();

                link.linked_ = true;
            } else {
                // Jump to dispatch for now :((
                emit_pc_flush(link.to_);
                B(dispatch_ent_for_block_);

                // Maybe reserved a word for unlink?
                NOP();

                link.linked_ = false;
            }
        }
    }

    void dashixiong_block::emit_return_to_dispatch(translated_block *block) {
        emit_cpsr_save();
        emit_cycles_count_add(block->inst_count_);

        B(dispatch_ent_for_block_);
    }

    enum thumb_instruction_size {
        THUMB_INST_SIZE_THUMB16 = 2,
        THUMB_INST_SIZE_THUMB32 = 4
    };

    // Thumb reading code is from Dynarmic, adjusted to this codebase. Thank you!
    static bool is_thumb16(const std::uint16_t first_part) {
        return (first_part & 0xF800) <= 0xE800;
    }

    static std::optional<std::pair<std::uint32_t, thumb_instruction_size>> read_thumb_instruction(const vaddress arm_pc,
        memory_operation_32bit_func &read_code) {
        std::uint32_t first_part = 0;
        if (!read_code(arm_pc & 0xFFFFFFFC, &first_part)) {
            return std::nullopt;
        }

        if ((arm_pc & 0x2) != 0) {
            first_part >>= 16;
        }

        first_part &= 0xFFFF;

        if (is_thumb16(static_cast<std::uint16_t>(first_part))) {
            // 16-bit thumb instruction
            return std::make_pair(first_part, THUMB_INST_SIZE_THUMB16);
        }

        // 32-bit thumb instruction
        // These always start with 0b11101, 0b11110 or 0b11111.
        std::uint32_t second_part = 0;
        if (!read_code((arm_pc + 2) & 0xFFFFFFFC, &second_part)) {
            return std::nullopt;
        }

        if (((arm_pc + 2) & 0x2) != 0) {
            second_part >>= 16;
        }

        second_part &= 0xFFFF;

        return std::make_pair(static_cast<std::uint32_t>((first_part << 16) | second_part),
             THUMB_INST_SIZE_THUMB32);
    }

    translated_block *dashixiong_block::compile_new_block(core_state *state, const vaddress addr) {
        translated_block *block = start_new_block(addr, state->current_aid_);
        if (!block) {
            return nullptr;
        }

        if (addr == 0x503AC148) {
            LOG_TRACE(CPU_12L1R, "0x{:X}", state->cpsr_);
        }
        const bool is_thumb = (state->cpsr_ & CPSR_THUMB_FLAG_MASK);
        bool should_continue = false;

		block->thumb_ = is_thumb;

        LOG_TRACE(CPU_12L1R, "Compiling new block PC=0x{:X}, host=0x{:X}, thumb={}", addr,
            reinterpret_cast<std::uint32_t>(block->translated_code_), is_thumb);

        std::unique_ptr<arm_translate_visitor> arm_visitor = nullptr;
        std::unique_ptr<thumb_translate_visitor> thumb_visitor = nullptr;

        if (!is_thumb) {
            arm_visitor = std::make_unique<arm_translate_visitor>(this, block);
        } else {
            thumb_visitor = std::make_unique<thumb_translate_visitor>(this, block);
        }

        begin_write();

        // Let them know the address damn
        emit_pc_flush(addr);

        // Load CPSR into register
        LDR(CPSR_REG, CORE_STATE_REG, offsetof(core_state, cpsr_));

        // Emit the check if we should go outside and stop running
        // Check if we are out of cycles, set the should_break if we should stop and
        // return to dispatch
        LDR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, ticks_left_));
        CMP(ALWAYS_SCRATCH1, 0);

        set_cc(common::CC_LE);

        {
            // Set should break and return to dispatch
            MOVI2R(ALWAYS_SCRATCH1, 1);
            STR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, should_break_));

            B(dispatch_ent_for_block_);
        }

        set_cc(common::CC_AL);

        // Restore CPSR here.
        if (arm_visitor) {
            arm_visitor->emit_cpsr_restore_nzcv();
        } else {
            thumb_visitor->emit_cpsr_restore_nzcv();
        }

        std::uint32_t inst = 0;

        do {
            std::uint32_t inst_size = 0;

            if (is_thumb) {
                auto read_res = read_thumb_instruction(addr + block->size_, callbacks_.code_read_);

                if (!read_res) {
                    LOG_ERROR(CPU_12L1R, "Error while reading instruction at address 0x{:X}!, addr");
                    return nullptr;
                }

                inst = read_res->first;

                if (read_res->second == THUMB_INST_SIZE_THUMB32) {
                    if (auto decoder = decode_thumb32<thumb_translate_visitor>(inst)) {
                        should_continue = decoder->get().call(*thumb_visitor, inst);
                    } else {
                        should_continue = thumb_visitor->thumb32_UDF();
                    }
                } else {
                    if (auto decoder = decode_thumb16<thumb_translate_visitor>(static_cast<std::uint16_t>(inst))) {
                        should_continue = decoder->get().call(*thumb_visitor, inst);
                    } else {
                        should_continue = thumb_visitor->thumb16_UDF();
                    }
                }

                inst_size = read_res->second;
            } else {
                if (!callbacks_.code_read_(addr + block->size_, &inst)) {
                    LOG_ERROR(CPU_12L1R, "Error while reading instruction at address 0x{:X}!, addr");
                    return nullptr;
                }

                // ARM decoding. NEON -> VFP -> ARM
                if (auto decoder = decode_arm<arm_translate_visitor>(inst)) {
                    should_continue = decoder->get().call(*arm_visitor, inst);
                } else {
                    should_continue = arm_visitor->arm_UDF();
                }

                inst_size = 4;
            }

            block->size_ += inst_size;
            block->last_inst_size_ = inst_size;
            block->inst_count_++;
        } while (should_continue);

        if (is_thumb) {
            thumb_visitor->finalize();
        } else {
            arm_visitor->finalize();
        }

        end_write();
        flush_icache();

        return block;
    }
}