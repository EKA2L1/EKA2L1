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

#include <cpu/12l1r/arm_12l1r.h>
#include <cpu/12l1r/block_gen.h>
#include <cpu/12l1r/common.h>
#include <cpu/12l1r/core_state.h>
#include <cpu/12l1r/exclusive_monitor.h>
#include <cpu/12l1r/reg_loc.h>

#include <cpu/12l1r/arm_visitor.h>
#include <cpu/12l1r/encoding/arm.h>
#include <cpu/12l1r/encoding/thumb16.h>
#include <cpu/12l1r/encoding/thumb32.h>
#include <cpu/12l1r/encoding/vfp.h>
#include <cpu/12l1r/thumb_visitor.h>
#include <cpu/12l1r/visit_session.h>

#include <common/algorithm.h>
#include <common/log.h>

namespace eka2l1::arm::r12l1 {
    static constexpr std::size_t MAX_CODE_SPACE_BYTES = common::MB(32);

    static translated_block *dashixiong_get_block_proxy(dashixiong_block *self, const vaddress addr) {
        return self->get_block(addr);
    }

    static translated_block *dashixiong_compile_new_block_proxy(dashixiong_block *self, core_state *state, const vaddress addr) {
        return self->compile_new_block(state, addr);
    }

    static void emit_pc_flush_with_this_emitter(common::armgen::armx_emitter *emitter, const address current_pc) {
        emitter->MOVI2R(ALWAYS_SCRATCH1, current_pc);
        emitter->STR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, gprs_[15]));
    }

    dashixiong_block::dashixiong_block(r12l1_core *parent)
        : dispatch_func_(nullptr)
        , dispatch_ent_for_block_(nullptr)
        , flags_(0)
        , parent_(parent) {
        context_info.detect();
        clear_fast_dispatch();

        alloc_codespace(MAX_CODE_SPACE_BYTES);
        assemble_control_funcs();

        cache_.set_on_block_invalidate_callback([this](translated_block *to_destroy) {
            edit_block_links(to_destroy, true);

            // Remove this block on fast dispatch table
            const std::uint32_t fast_dispatch_index = (to_destroy->start_address() >> FAST_DISPATCH_ENTRY_ADDR_SHIFT)
                & FAST_DISPATCH_ENTRY_MASK;
            fast_dispatch_entry &entry_may_empty = fast_dispatches_[fast_dispatch_index];

            if (entry_may_empty.addr_ == to_destroy->start_address()) {
                // Empty out this entry
                std::memset(&entry_may_empty, 0, sizeof(fast_dispatch_entry));
            }

            // Remove all related link instance of this
            for (std::uint32_t i = 0; i < to_destroy->links_.size(); i++) {
                auto link_ites = link_to_.equal_range(to_destroy->links_[i].to_);

                for (auto ite = link_ites.first; ite != link_ites.second; ite++) {
                    if (ite->second == to_destroy) {
                        link_to_.erase(ite);
                        break;
                    }
                }
            }
        });
    }

    static void dashixiong_print_debug(const std::uint32_t val) {
        LOG_TRACE(CPU_12L1R, "Print debug: 0x{:X}", val);
    }

    void dashixiong_block::clear_fast_dispatch() {
        std::memset(fast_dispatches_.data(), 0, sizeof(fast_dispatch_entry) * FAST_DISPATCH_ENTRY_COUNT);
    }

    void dashixiong_block::assemble_control_funcs() {
        begin_write();
        dispatch_func_ = get_writeable_code_ptr();

        // Load the arguments to call lookup
        // While R0 is already the core state
        PUSH(9, common::armgen::R4, common::armgen::R5, common::armgen::R6, common::armgen::R7, common::armgen::R8,
            common::armgen::R9, common::armgen::R10, common::armgen::R11, common::armgen::R14);

        // 8 bytes stack alignment, go die in hell
        SUB(common::armgen::R_SP, common::armgen::R_SP, 4);

        // Load core state reg, ticks left and CPSR
        MOV(CORE_STATE_REG, common::armgen::R0);
        LDR(TICKS_REG, CORE_STATE_REG, offsetof(core_state, ticks_left_));

        emit_cpsr_load();
        emit_fpscr_save(true);
        emit_fpscr_load(false);

        dispatch_ent_for_block_ = get_code_ptr();

        LDR(common::armgen::R1, CORE_STATE_REG, offsetof(core_state, should_break_));
        CMPI2R(common::armgen::R1, 0, common::armgen::R12);

        auto return_back = B_CC(common::CC_NEQ);

        LDR(common::armgen::R4, CORE_STATE_REG, offsetof(core_state, gprs_[15]));

        auto emit_get_block_code = [&]() -> common::armgen::fixup_branch {
            MOV(common::armgen::R1, common::armgen::R4);
            MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(this));

            quick_call_function(common::armgen::R12, reinterpret_cast<void *>(dashixiong_get_block_proxy));

            CMPI2R(common::armgen::R0, 0, common::armgen::R12);
            common::armgen::fixup_branch available = B_CC(common::CC_NEQ);

            // Call for recompile
            MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(this));
            MOV(common::armgen::R1, CORE_STATE_REG);
            MOV(common::armgen::R2, common::armgen::R4);

            quick_call_function(common::armgen::R12, reinterpret_cast<void *>(dashixiong_compile_new_block_proxy));
            CMPI2R(common::armgen::R0, 0, common::armgen::R12);

            common::armgen::fixup_branch available_again = B_CC(common::CC_NEQ);

            // first set the jit state to break. 2 indicates error happened.
            // then exit (TODO)
            MOVI2R(common::armgen::R0, 2);
            STR(common::armgen::R0, CORE_STATE_REG, offsetof(core_state, should_break_));
            common::armgen::fixup_branch headout = B();

            set_jump_target(available);
            set_jump_target(available_again);

            return headout;
        };

        common::armgen::fixup_branch headout = emit_get_block_code();

        LDR(common::armgen::R0, common::armgen::R0, offsetof(translated_block, translated_code_));
        B(common::armgen::R0); // Branch to the block

        set_jump_target(headout);
        set_jump_target(return_back);

        auto emit_wrap_up_dispatch = [&]() {
            // Save the CPSR and ticks
            emit_cpsr_save();
            emit_fpscr_load(true);
            emit_cycles_count_save();

            // Restore alignment
            ADD(common::armgen::R_SP, common::armgen::R_SP, 4);

            // Branch back to where it went
            POP(9, common::armgen::R4, common::armgen::R5, common::armgen::R6, common::armgen::R7, common::armgen::R8,
                common::armgen::R9, common::armgen::R10, common::armgen::R11, common::armgen::R15);
        };

        emit_wrap_up_dispatch();
        flush_lit_pool();
        end_write();

        begin_write();
        fast_dispatch_ent_ = get_code_ptr();

        // Check if we should break
        LDR(common::armgen::R1, CORE_STATE_REG, offsetof(core_state, should_break_));
        CMPI2R(common::armgen::R1, 0, common::armgen::R12);

        return_back = B_CC(common::CC_NEQ);

        // Core state will load the current address space ID
        LDR(common::armgen::R0, CORE_STATE_REG, offsetof(core_state, gprs_[15]));

        MOVI2R(ALWAYS_SCRATCH2, FAST_DISPATCH_ENTRY_MASK);
        AND(ALWAYS_SCRATCH2, ALWAYS_SCRATCH2, common::armgen::operand2(common::armgen::R0, common::armgen::ST_LSR, FAST_DISPATCH_ENTRY_ADDR_SHIFT));

        // Indexing the dispatch entry
        static_assert(sizeof(fast_dispatch_entry) == 8);
        LSL(ALWAYS_SCRATCH2, ALWAYS_SCRATCH2, 3);
        ADDI2R(ALWAYS_SCRATCH2, ALWAYS_SCRATCH2, reinterpret_cast<std::uint32_t>(fast_dispatches_.data()),
            common::armgen::R1);

        LDR(common::armgen::R1, ALWAYS_SCRATCH2, offsetof(fast_dispatch_entry, addr_));

        // Comparing the address and address space
        CMP(common::armgen::R0, common::armgen::R1);
        auto fast_dispatch_miss = B_CC(common::CC_NEQ);

        // Branch to the dispatched code...
        LDR(ALWAYS_SCRATCH2, ALWAYS_SCRATCH2, offsetof(fast_dispatch_entry, code_));
        B(ALWAYS_SCRATCH2);

        set_jump_target(fast_dispatch_miss);

        MOV(common::armgen::R4, common::armgen::R0);
        MOV(common::armgen::R6, ALWAYS_SCRATCH2); // Store base of entry.

        common::armgen::fixup_branch no_code = emit_get_block_code();

        // Add to fast dispatch, then execute the block
        STR(common::armgen::R4, common::armgen::R6, offsetof(fast_dispatch_entry, addr_));
        LDR(ALWAYS_SCRATCH1, common::armgen::R0, offsetof(translated_block, translated_code_));
        STR(ALWAYS_SCRATCH1, common::armgen::R6, offsetof(fast_dispatch_entry, code_));

        B(ALWAYS_SCRATCH1);

        set_jump_target(no_code);
        set_jump_target(return_back);

        emit_wrap_up_dispatch();

        flush_lit_pool();
        end_write();
    }

    translated_block *dashixiong_block::start_new_block(const vaddress addr) {
        bool try_new_block_result = cache_.add_block(addr);
        if (!try_new_block_result) {
            LOG_ERROR(CPU_12L1R, "Trying to start a block that already exists!");
            return nullptr;
        }

        translated_block *blck = cache_.lookup_block(addr);
        blck->translated_code_ = get_code_pointer();

        return blck;
    }

    void dashixiong_block::edit_block_links(translated_block *dest, bool unlink) {
        auto link_ites = link_to_.equal_range(dest->hash_);
        const auto psize = common::get_host_page_size();

        for (auto ite = link_ites.first; ite != link_ites.second; ite++) {
            translated_block *request_link_block = ite->second;

            for (auto &link : request_link_block->links_) {
                if (link.to_ == dest->start_address()) {
                    void *aligned_addr = common::align_address_to_host_page(link.value_);

                    if (common::is_memory_wx_exclusive()) {
                        common::change_protection(aligned_addr, psize * 2, prot_read_write);
                    }

                    common::cpu_info temp_info = context_info;
                    common::armgen::armx_emitter temp_emitter(reinterpret_cast<std::uint8_t *>(link.value_),
                        temp_info);

                    if (unlink) {
                        emit_pc_flush_with_this_emitter(&temp_emitter, link.to_);
                        temp_emitter.B(dispatch_ent_for_block_);

                        link.linked_ = false;
                    } else {
                        temp_emitter.B(dest->translated_code_);
                        link.linked_ = true;
                    }

                    temp_emitter.flush_lit_pool();
                    temp_emitter.flush_icache();

                    if (common::is_memory_wx_exclusive()) {
                        common::change_protection(aligned_addr, psize * 2, prot_read_exec);
                    }
                }
            }
        }
    }

    translated_block *dashixiong_block::get_block(const vaddress addr) {
        return cache_.lookup_block(addr);
    }

    void dashixiong_block::flush_range(const vaddress start, const vaddress end) {
        cache_.flush_range(start, end);
    }

    void dashixiong_block::flush_all() {
        clear_fast_dispatch();

        link_to_.clear();
        cache_.flush_all();

        clear_codespace(0);
        assemble_control_funcs();
    }

    bool dashixiong_block::raise_guest_exception(const exception_type exc, const std::uint32_t usrdata) {
        return parent_->exception_handler(exc, usrdata);
    }

    void dashixiong_block::raise_system_call(const std::uint32_t num) {
        parent_->system_call_handler(num);
    }

    std::uint8_t dashixiong_block::read_byte(const vaddress addr) {
        std::uint8_t value = 0;
        bool res = parent_->read_8bit(addr, &value);

        if (!res) {
            if (raise_guest_exception(exception_type_access_violation_read, addr)) {
                res = parent_->read_8bit(addr, &value);
            }
        }

        if (!res) {
            LOG_ERROR(CPU_12L1R, "Failed to read BYTE at address 0x{:X}", addr);
        }

        return value;
    }

    std::uint16_t dashixiong_block::read_word(const vaddress addr) {
        std::uint16_t value = 0;
        bool res = parent_->read_16bit(addr, &value);

        if (!res) {
            if (raise_guest_exception(exception_type_access_violation_read, addr)) {
                res = parent_->read_16bit(addr, &value);
            }
        }

        if (!res) {
            LOG_ERROR(CPU_12L1R, "Failed to read WORD at address 0x{:X}", addr);
        }

        return value;
    }

    std::uint32_t dashixiong_block::read_dword(const vaddress addr) {
        std::uint32_t value = 0;
        bool res = parent_->read_32bit(addr, &value);

        if (!res) {
            if (raise_guest_exception(exception_type_access_violation_read, addr)) {
                res = parent_->read_32bit(addr, &value);
            }
        }

        if (!res) {
            LOG_ERROR(CPU_12L1R, "Failed to read DWORD at address 0x{:X}", addr);
        }

        return value;
    }

    std::uint64_t dashixiong_block::read_qword(const vaddress addr) {
        std::uint64_t value = 0;
        bool res = parent_->read_64bit(addr, &value);

        if (!res) {
            if (raise_guest_exception(exception_type_access_violation_read, addr)) {
                res = parent_->read_64bit(addr, &value);
            }
        }

        if (!res) {
            LOG_ERROR(CPU_12L1R, "Failed to read QWORD at address 0x{:X}", addr);
        }

        return value;
    }

    void dashixiong_block::write_byte(const vaddress addr, std::uint8_t dat) {
        bool result = parent_->write_8bit(addr, &dat);

        if (!result) {
            if (raise_guest_exception(exception_type_access_violation_write, addr)) {
                result = parent_->write_8bit(addr, &dat);
            }
        }

        if (!result) {
            LOG_ERROR(CPU_12L1R, "Failed to write BYTE to address 0x{:X}", addr);
        }
    }

    void dashixiong_block::write_word(const vaddress addr, std::uint16_t dat) {
        bool result = parent_->write_16bit(addr, &dat);

        if (!result) {
            if (raise_guest_exception(exception_type_access_violation_write, addr)) {
                result = parent_->write_16bit(addr, &dat);
            }
        }

        if (!result) {
            LOG_ERROR(CPU_12L1R, "Failed to write WORD to address 0x{:X}", addr);
        }
    }

    void dashixiong_block::write_dword(const vaddress addr, std::uint32_t dat) {
        bool result = parent_->write_32bit(addr, &dat);

        if (!result) {
            if (raise_guest_exception(exception_type_access_violation_write, addr)) {
                result = parent_->write_32bit(addr, &dat);
            }
        }

        if (!result) {
            LOG_ERROR(CPU_12L1R, "Failed to write DWORD to address 0x{:X}", addr);
        }
    }

    void dashixiong_block::write_qword(const vaddress addr, std::uint64_t dat) {
        bool result = parent_->write_64bit(addr, &dat);

        if (!result) {
            if (raise_guest_exception(exception_type_access_violation_write, addr)) {
                result = parent_->write_64bit(addr, &dat);
            }
        }

        if (!result) {
            LOG_ERROR(CPU_12L1R, "Failed to write QWORD to address 0x{:X}", addr);
        }
    }

    std::uint8_t dashixiong_block::read_and_mark_byte(const vaddress addr) {
        return parent_->monitor_->read_and_mark<std::uint8_t>(parent_->core_number(), addr, [&]() -> std::uint8_t {
            // TODO: Check status
            std::uint8_t val = 0;
            parent_->read_8bit(addr, &val);

            return val;
        });
    }

    std::uint16_t dashixiong_block::read_and_mark_word(const vaddress addr) {
        return parent_->monitor_->read_and_mark<std::uint16_t>(parent_->core_number(), addr, [&]() -> std::uint16_t {
            // TODO: Check status
            std::uint16_t val = 0;
            parent_->read_16bit(addr, &val);

            return val;
        });
    }

    std::uint32_t dashixiong_block::read_and_mark_dword(const vaddress addr) {
        return parent_->monitor_->read_and_mark<std::uint32_t>(parent_->core_number(), addr, [&]() -> std::uint32_t {
            // TODO: Check status
            std::uint32_t val = 0;
            parent_->read_32bit(addr, &val);

            return val;
        });
    }

    std::uint64_t dashixiong_block::read_and_mark_qword(const vaddress addr) {
        return parent_->monitor_->read_and_mark<std::uint64_t>(parent_->core_number(), addr, [&]() -> std::uint64_t {
            // TODO: Check status
            std::uint64_t val = 0;
            parent_->read_64bit(addr, &val);

            return val;
        });
    }

    // Note: 0 means success! Need to convert
    bool dashixiong_block::write_ex_byte(const vaddress addr, const std::uint8_t val) {
        return parent_->monitor_->do_exclusive_operation<std::uint8_t>(parent_->core_number(), addr,
                   [&](std::uint8_t expected) -> bool {
#if R12L1_ENABLE_FUZZ
                       if (flags_ & FLAG_ENABLE_FUZZ) {
                           return static_cast<std::int32_t>(interpreter_monitor_->exclusive_operation_results_[parent_->core_number()]);
                       }
#endif
                       return (parent_->exclusive_write_8bit(addr, val, expected) > 0);
                   })
            ? 0
            : 1;
    }

    bool dashixiong_block::write_ex_word(const vaddress addr, const std::uint16_t val) {
        return parent_->monitor_->do_exclusive_operation<std::uint16_t>(parent_->core_number(), addr,
                   [&](std::uint16_t expected) -> bool {
#if R12L1_ENABLE_FUZZ
                       if (flags_ & FLAG_ENABLE_FUZZ) {
                           return static_cast<std::int32_t>(interpreter_monitor_->exclusive_operation_results_[parent_->core_number()]);
                       }
#endif
                       return (parent_->exclusive_write_16bit(addr, val, expected) > 0);
                   })
            ? 0
            : 1;
    }

    bool dashixiong_block::write_ex_dword(const vaddress addr, const std::uint32_t val) {
        return parent_->monitor_->do_exclusive_operation<std::uint32_t>(parent_->core_number(), addr,
                   [&](std::uint32_t expected) -> bool {
#if R12L1_ENABLE_FUZZ
                       if (flags_ & FLAG_ENABLE_FUZZ) {
                           return static_cast<std::int32_t>(interpreter_monitor_->exclusive_operation_results_[parent_->core_number()]);
                       }
#endif
                       return (parent_->exclusive_write_32bit(addr, val, expected) > 0);
                   })
            ? 0
            : 1;
    }

    bool dashixiong_block::write_ex_qword(const vaddress addr, const std::uint64_t val) {
        return parent_->monitor_->do_exclusive_operation<std::uint64_t>(parent_->core_number(), addr,
                   [&](std::uint64_t expected) -> bool {
#if R12L1_ENABLE_FUZZ
                       if (flags_ & FLAG_ENABLE_FUZZ) {
                           return static_cast<std::int32_t>(interpreter_monitor_->exclusive_operation_results_[parent_->core_number()]);
                       }
#endif
                       return (parent_->exclusive_write_64bit(addr, val, expected) > 0);
                   })
            ? 0
            : 1;
    }

    void dashixiong_block::enter_dispatch(core_state *cstate) {
        typedef void (*dispatch_func_type)(core_state * state);
        dispatch_func_type df = reinterpret_cast<dispatch_func_type>(dispatch_func_);

        df(cstate);
    }

    void dashixiong_block::emit_cycles_count_add(const std::uint32_t num) {
        SUBI2R(TICKS_REG, TICKS_REG, num, ALWAYS_SCRATCH1);
    }

    void dashixiong_block::emit_pc_flush(const address current_pc) {
        emit_pc_flush_with_this_emitter(this, current_pc);
    }

    void dashixiong_block::emit_cpsr_save() {
        STR(CPSR_REG, CORE_STATE_REG, offsetof(core_state, cpsr_));
    }

    void dashixiong_block::emit_cpsr_load() {
        LDR(CPSR_REG, CORE_STATE_REG, offsetof(core_state, cpsr_));
    }

    void dashixiong_block::emit_fpscr_save(const bool save_host) {
        VMRS(ALWAYS_SCRATCH1);
        if (!save_host) {
            LDR(ALWAYS_SCRATCH2, CORE_STATE_REG, offsetof(core_state, fpscr_));
            AND(ALWAYS_SCRATCH2, ALWAYS_SCRATCH2, 0b110111 << 16);
            ORR(ALWAYS_SCRATCH2, ALWAYS_SCRATCH2, ALWAYS_SCRATCH1);
            STR(ALWAYS_SCRATCH2, CORE_STATE_REG, offsetof(core_state, fpscr_));
        } else {
            STR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, fpscr_host_));
        }
    }

    void dashixiong_block::emit_fpscr_load(const bool load_host) {
        LDR(ALWAYS_SCRATCH1, CORE_STATE_REG, load_host ? offsetof(core_state, fpscr_host_) : offsetof(core_state, fpscr_));
        BIC(ALWAYS_SCRATCH1, ALWAYS_SCRATCH1, 0x370000);
        VMSR(ALWAYS_SCRATCH1);
    }

    void dashixiong_block::emit_cycles_count_save() {
        STR(TICKS_REG, CORE_STATE_REG, offsetof(core_state, ticks_left_));
    }

    void dashixiong_block::emit_pc_write_exchange(common::armgen::arm_reg pc_reg) {
        MOV(ALWAYS_SCRATCH1, pc_reg);
        BIC(CPSR_REG, CPSR_REG, CPSR_THUMB_FLAG_MASK); // Clear T flag

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

    void dashixiong_block::emit_pc_write(common::armgen::arm_reg pc_reg) {
        // Have to align it smh
        BIC(ALWAYS_SCRATCH1, pc_reg, 1);
        STR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, gprs_[15]));
    }

    void dashixiong_block::emit_block_links(translated_block *block) {
        for (auto &link : block->links_) {
            for (auto &jump_target : link.needs_) {
                set_jump_target(jump_target);
            }

            link_to_.emplace(link.to_, block);
            link.value_ = reinterpret_cast<std::uint32_t *>(get_writeable_code_ptr());

            // Can we link the block now?
            if (auto link_block = get_block(link.to_)) {
                B(link_block->translated_code_);

                // Reserved three words for unlink?
                NOP();
                NOP();
                NOP();

                link.linked_ = true;
            } else {
                // Jump to dispatch for now :((
                emit_pc_flush(link.to_);
                B(dispatch_ent_for_block_);

                link.linked_ = false;
            }
        }
    }

    void dashixiong_block::emit_return_to_dispatch(translated_block *block, const bool fast_hint) {
        if (fast_hint)
            B(fast_dispatch_ent_);
        else
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
#if R12L1_ENABLE_FUZZ
        if (get_space_left() <= THRESHOLD_LEFT_TO_RESET_CACHE_FUZZ) {
#else
        if (get_space_left() <= THRESHOLD_LEFT_TO_RESET_CACHE) {
#endif
            flush_all();
        }

        translated_block *block = start_new_block(addr);
        if (!block) {
            return nullptr;
        }

        // When you want to start the fuzz, call fuzz_start(), and end it with fuzz_end()
        //if (addr == 0x70393258)
        //   fuzz_start();

        const bool is_thumb = (state->cpsr_ & CPSR_THUMB_FLAG_MASK);
        bool should_continue = false;

        block->thumb_ = is_thumb;
        current_fpscr_ = state->fpscr_;

        // LOG_TRACE(CPU_12L1R, "Compiling new block PC=0x{:X}, host=0x{:X}, thumb={}", addr,
        //     reinterpret_cast<std::uint32_t>(block->translated_code_), is_thumb);

        std::unique_ptr<visit_session> visitor = nullptr;

        if (!is_thumb) {
            visitor = std::make_unique<arm_translate_visitor>(this, block);
        } else {
            visitor = std::make_unique<thumb_translate_visitor>(this, block);
        }

        // Reserve 512 pages for these JIT codes. When fuzz is enabled this is probably larger
#if R12L1_ENABLE_FUZZ
        begin_write(1024);
#else
        begin_write(512);
#endif

        emit_pc_flush(addr);

        // Emit the check if we should go outside and stop running
        // Check if we are out of cycles, set the should_break if we should stop and
        // return to dispatch
        CMP(TICKS_REG, 0);

        set_cc(common::CC_LE);

        {
            // Set should break and return to dispatch
            MOVI2R(ALWAYS_SCRATCH1, 1);
            STR(ALWAYS_SCRATCH1, CORE_STATE_REG, offsetof(core_state, should_break_));

            B(dispatch_ent_for_block_);
        }

        set_cc(common::CC_AL);

        // Restore CPSR here.
        visitor->emit_cpsr_restore_nzcvq();

        std::uint32_t inst = 0;

        do {
            std::uint32_t inst_size = 0;

            if (is_thumb) {
                auto read_res = read_thumb_instruction(addr + block->size_, parent_->read_code);

                if (!read_res) {
                    LOG_ERROR(CPU_12L1R, "Error while reading instruction at address 0x{:X}!", addr);
                    return nullptr;
                }

                inst = read_res->first;
                inst_size = read_res->second;
            } else {
                if (!parent_->read_code(addr + block->size_, &inst)) {
                    LOG_ERROR(CPU_12L1R, "Error while reading instruction at address 0x{:X}!", addr);
                    return nullptr;
                }

                inst_size = 4;
            }

#if R12L1_ENABLE_FUZZ
            visitor->emit_fuzzing_execs(is_thumb ? (inst_size / 2) : 1);
#endif

            if (is_thumb) {
                if (inst_size == THUMB_INST_SIZE_THUMB32) {
                    if (auto decoder = decode_thumb32<thumb_translate_visitor>(inst)) {
                        should_continue = decoder->get().call(static_cast<thumb_translate_visitor &>(*visitor), inst);
                    } else {
                        should_continue = static_cast<thumb_translate_visitor &>(*visitor).thumb32_UDF();
                    }
                } else {
                    if (auto decoder = decode_thumb16<thumb_translate_visitor>(static_cast<std::uint16_t>(inst))) {
                        should_continue = decoder->get().call(static_cast<thumb_translate_visitor &>(*visitor), inst);
                    } else {
                        should_continue = static_cast<thumb_translate_visitor &>(*visitor).thumb16_UDF();
                    }
                }
            } else {
                // ARM decoding. NEON -> VFP -> ARM
                if (auto decoder = decode_vfp<arm_translate_visitor>(inst)) {
                    should_continue = decoder->get().call(static_cast<arm_translate_visitor &>(*visitor), inst);
                } else if (auto decoder = decode_arm<arm_translate_visitor>(inst)) {
                    should_continue = decoder->get().call(static_cast<arm_translate_visitor &>(*visitor), inst);
                } else {
                    should_continue = static_cast<arm_translate_visitor &>(*visitor).arm_UDF();
                }

                inst_size = 4;
            }

            visitor->cycle_next(inst_size);
        } while (should_continue);

        visitor->finalize();

        end_write();
        flush_icache();

        return block;
    }

#if R12L1_ENABLE_FUZZ
    void dashixiong_block::fuzz_start() {
        if (!(flags_ & FLAG_ENABLE_FUZZ)) {
            if (!interpreter_) {
                interpreter_monitor_ = std::make_unique<exclusive_monitor>(parent_->monitor_->get_processor_count());
                interpreter_monitor_->read_8bit = [this](core *cc, address addr, std::uint8_t *dat) {
                    return parent_->monitor_->read_8bit(parent_, addr, dat);
                };

                interpreter_monitor_->read_16bit = [this](core *cc, address addr, std::uint16_t *dat) {
                    return parent_->monitor_->read_16bit(parent_, addr, dat);
                };

                interpreter_monitor_->read_32bit = [this](core *cc, address addr, std::uint32_t *dat) {
                    return parent_->monitor_->read_32bit(parent_, addr, dat);
                };

                interpreter_monitor_->read_64bit = [this](core *cc, address addr, std::uint64_t *dat) {
                    return parent_->monitor_->read_64bit(parent_, addr, dat);
                };

                interpreter_monitor_->write_8bit = [&](core *cc, address addr, std::uint8_t v1, std::uint8_t v2) -> std::int32_t {
                    return parent_->monitor_->write_8bit(parent_, addr, v1, v2);
                };
                interpreter_monitor_->write_16bit = [&](core *cc, address addr, std::uint16_t v1, std::uint16_t v2) -> std::int32_t {
                    return parent_->monitor_->write_16bit(parent_, addr, v1, v2);
                };
                interpreter_monitor_->write_32bit = [&](core *cc, address addr, std::uint32_t v1, std::uint32_t v2) -> std::int32_t {
                    return parent_->monitor_->write_32bit(parent_, addr, v1, v2);
                };
                interpreter_monitor_->write_64bit = [&](core *cc, address addr, std::uint64_t v1, std::uint64_t v2) -> std::int32_t {
                    return parent_->monitor_->write_64bit(parent_, addr, v1, v2);
                };

                interpreter_ = std::make_unique<dyncom_core>(interpreter_monitor_.get(), parent_->mem_cache_.page_bits);
                dyncom_core *interpreter_ptr = interpreter_.get();

                // Copy lengthy callbacks
                interpreter_->read_8bit = parent_->read_8bit;
                interpreter_->read_16bit = parent_->read_16bit;
                interpreter_->read_32bit = parent_->read_32bit;
                interpreter_->read_64bit = parent_->read_64bit;
                interpreter_->read_code = parent_->read_code;
                interpreter_->write_8bit = parent_->write_8bit;
                interpreter_->write_16bit = parent_->write_16bit;
                interpreter_->write_32bit = parent_->write_32bit;
                interpreter_->write_64bit = parent_->write_64bit;
                interpreter_->exclusive_write_8bit = parent_->exclusive_write_8bit;
                interpreter_->exclusive_write_16bit = parent_->exclusive_write_16bit;
                interpreter_->exclusive_write_32bit = parent_->exclusive_write_32bit;
                interpreter_->exclusive_write_64bit = parent_->exclusive_write_64bit;
                interpreter_->system_call_handler = [this](const std::uint32_t num) {
                    flags_ |= FLAG_FUZZ_LAST_SYSCALL;
                };
                interpreter_->exception_handler = [interpreter_ptr](const arm::exception_type type, const std::uint32_t data) {
                    if ((type == arm::exception_type_access_violation_write) || (type == arm::exception_type_access_violation_read)) {
                        LOG_TRACE(CPU_12L1R, "Failure happens at 0x{:X}, back at 0x{:X}", interpreter_ptr->get_pc(), interpreter_ptr->get_lr());
                    }

                    return false;
                };
            }

            core::thread_context ctx;

            parent_->save_context(ctx);
            interpreter_->load_context(ctx);

            flags_ |= FLAG_ENABLE_FUZZ;
        }
    }

    std::uint32_t last_pc = 0;

    bool dashixiong_block::fuzz_execute() {
        if (flags_ & FLAG_ENABLE_FUZZ) {
            if (fuzz_svc_handle()) {
                return true;
            }

            interpreter_->step();
            return true;
        }

        return false;
    }

    bool dashixiong_block::fuzz_svc_handle() {
        if (flags_ & FLAG_FUZZ_LAST_SYSCALL) {
            // Restore to core state
            core::thread_context ctx;

            parent_->save_context(ctx);
            interpreter_->load_context(ctx);

            flags_ &= ~FLAG_FUZZ_LAST_SYSCALL;

            // Usually SVC ends a block, which make us, when we encounter this flag, one step behind
            // the current instruction!
            interpreter_->step();

            return true;
        }

        return false;
    }

    void dashixiong_block::fuzz_compare(core_state *state) {
        if (!(flags_ & FLAG_ENABLE_FUZZ)) {
            return;
        }

        if (fuzz_svc_handle()) {
            return;
        }

        // Compare CPSR NZCV
        static constexpr std::uint32_t NZCV_T_MASK = 0b11110000000000000000000000100000;
        if ((state->cpsr_ & NZCV_T_MASK) != (interpreter_->get_cpsr() & NZCV_T_MASK)) {
            LOG_ERROR(CPU_12L1R, "At PC: 0x{:X}, CPSR NZCV_T value mismatch (JIT: {} vs Int: {})", state->gprs_[15], state->cpsr_, interpreter_->get_cpsr());
        }

        // Compare FPSCR NZCV
        static constexpr std::uint32_t FPSCR_NZCV_MASK = 0b11110000000000000000000000000000;
        if ((state->fpscr_ & FPSCR_NZCV_MASK) != (interpreter_->get_fpscr() & FPSCR_NZCV_MASK)) {
            LOG_ERROR(CPU_12L1R, "At PC: 0x{:X}, FPSCR NZCV value mismatch (JIT: {} vs Int: {})", state->gprs_[15], state->fpscr_, interpreter_->get_fpscr());
        }

        if (state->gprs_[15] != interpreter_->get_pc()) {
            LOG_ERROR(CPU_12L1R, "PC of JIT (0x{:X}) vs Interpreter (0x{:X}) mismatch!", state->gprs_[15], interpreter_->get_pc());
        }

        static constexpr std::uint8_t MAX_CMP_GPRS = 14;
        for (std::uint8_t i = 0; i < MAX_CMP_GPRS; i++) {
            if (state->gprs_[i] != interpreter_->get_reg(i)) {
                LOG_ERROR(CPU_12L1R, "At PC: 0x{:X}, R{} mismatch! (JIT: {} vs Interpreter {})", state->gprs_[15], i, state->gprs_[i], interpreter_->get_reg(i));
            }
        }

        static constexpr std::uint8_t MAX_CMP_FPRS = 64;
        for (std::uint8_t i = 0; i < MAX_CMP_FPRS; i++) {
            if (state->fprs_[i] != interpreter_->get_vfp(i)) {
                LOG_ERROR(CPU_12L1R, "At PC: 0x{:X}, S{} mismatch! (JIT: {} vs Interpreter {})", state->gprs_[15], i, state->fprs_[i], interpreter_->get_vfp(i));
            }
        }
    }

    void dashixiong_block::fuzz_end() {
        flags_ &= ~FLAG_ENABLE_FUZZ;
    }
#endif
}
