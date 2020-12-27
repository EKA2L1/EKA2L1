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

#include <cpu/12l1r/core_state.h>
#include <cpu/12l1r/visit_session.h>
#include <cpu/12l1r/block_gen.h>
#include <cpu/12l1r/tlb.h>

#include <common/bytes.h>

namespace eka2l1::arm::r12l1 {
    static common::cc_flags get_negative_cc_flags(common::cc_flags cc) {
        common::cc_flags neg_cc = cc;
        switch (neg_cc) {
            case common::CC_EQ:
                neg_cc = common::CC_NEQ;
                break;

            case common::CC_NEQ:
                neg_cc = common::CC_EQ;
                break;

            case common::CC_CS:
                neg_cc = common::CC_CC;
                break;

            case common::CC_CC:
                neg_cc = common::CC_CS;
                break;

            case common::CC_MI:
                neg_cc = common::CC_PL;
                break;

            case common::CC_PL:
                neg_cc = common::CC_MI;
                break;

            case common::CC_VS:
                neg_cc = common::CC_VC;
                break;

            case common::CC_VC:
                neg_cc = common::CC_VS;
                break;

            case common::CC_HI:
                neg_cc = common::CC_LS;
                break;

            case common::CC_LS:
                neg_cc = common::CC_HI;
                break;

            case common::CC_GE:
                neg_cc = common::CC_LT;
                break;

            case common::CC_LT:
                neg_cc = common::CC_GE;
                break;

            case common::CC_GT:
                neg_cc = common::CC_LE;
                break;

            case common::CC_LE:
                neg_cc = common::CC_GT;
                break;

            default:
                assert(false);
                break;
        }

        return neg_cc;
    }

    static void dashixiong_raise_exception_router(dashixiong_block *self, const exception_type exc,
                                                  const std::uint32_t data) {
        self->raise_guest_exception(exc, data);
    }

    static void dashixiong_raise_system_call(dashixiong_block *self, const std::uint32_t num) {
        self->raise_system_call(num);
    }

    static std::uint32_t dashixiong_read_dword_router(dashixiong_block *self, const vaddress addr) {
        return self->read_dword(addr);
    }

    static void dashixiong_write_dword_router(dashixiong_block *self, const vaddress addr, const std::uint32_t val) {
        self->write_dword(addr, val);
    }

    visit_session::visit_session(dashixiong_block *bro, translated_block *crr)
        : last_flag_(common::CC_AL)
        , cpsr_modified_(false)
        , big_block_(bro)
        , crr_block_(crr)
        , reg_supplier_(bro) {
    }
    
    void visit_session::set_cond(common::cc_flags cc) {
        if ((cc == last_flag_) && (!cpsr_modified_)) {
            return;
        }

        if (last_flag_ != common::CC_AL) {
            big_block_->set_jump_target(end_of_cond_);
        }

        if (cc != common::CC_AL) {
            common::cc_flags neg_cc = get_negative_cc_flags(cc);
            end_of_cond_ = big_block_->B_CC(neg_cc);
        }

        last_flag_ = cc;
        cpsr_modified_ = false;
    }

    // Should we hardcode this??? Hmmm
    static constexpr std::uint32_t CPAGE_BITS = 12;
    static constexpr std::uint32_t CPAGE_SIZE = 1 << CPAGE_BITS;
    static constexpr std::uint32_t CPAGE_MASK = CPAGE_SIZE - 1;

    common::armgen::arm_reg visit_session::emit_address_lookup(common::armgen::arm_reg base, const bool for_read) {
        static_assert(sizeof(tlb_entry) == 16);

        std::uint32_t offset_to_load = (for_read) ? offsetof(tlb_entry, read_addr) : offsetof(tlb_entry, write_addr);
        common::armgen::arm_reg final_addr = reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR);
        common::armgen::arm_reg scratch3 = reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR);

        big_block_->LSR(ALWAYS_SCRATCH1, base, CPAGE_BITS);
        big_block_->ANDI2R(ALWAYS_SCRATCH1, ALWAYS_SCRATCH1, TLB_ENTRY_MASK, ALWAYS_SCRATCH2);

        // Calculate the offset
        big_block_->LSL(ALWAYS_SCRATCH1, ALWAYS_SCRATCH1, 4);
        big_block_->LDR(ALWAYS_SCRATCH2, CORE_STATE_REG, offsetof(core_state, entries_));
        big_block_->ADD(ALWAYS_SCRATCH1, ALWAYS_SCRATCH1, ALWAYS_SCRATCH2);

        // Scratch 1 now holds our TLB entry
        big_block_->LDR(ALWAYS_SCRATCH2, ALWAYS_SCRATCH1, offset_to_load);
        big_block_->ANDI2R(scratch3, base, ~CPAGE_MASK, ALWAYS_SCRATCH2);     // remove the unaligned
        big_block_->CMP(scratch3, ALWAYS_SCRATCH2);

        big_block_->set_cc(common::CC_NEQ);
        big_block_->MOV(final_addr, 0);
        big_block_->set_cc(common::CC_AL);

        common::armgen::fixup_branch tlb_miss = big_block_->B_CC(common::CC_NEQ);

        // Load the address
        big_block_->LDR(final_addr, ALWAYS_SCRATCH1, offsetof(tlb_entry, host_base));
        big_block_->ANDI2R(ALWAYS_SCRATCH2, base, CPAGE_MASK, ALWAYS_SCRATCH1);
        big_block_->ADD(final_addr, final_addr, ALWAYS_SCRATCH2);

        big_block_->set_jump_target(tlb_miss);

        return final_addr;
    }

    static constexpr std::uint32_t NZCV_FLAG_MASK = 0b1111 << 28;

    void visit_session::emit_cpsr_update_nzcv() {
        big_block_->MRS(ALWAYS_SCRATCH1);

        // Filter out to only NZCV, and clear emulator nzcv bits
        big_block_->ANDI2R(ALWAYS_SCRATCH1, ALWAYS_SCRATCH1, NZCV_FLAG_MASK, ALWAYS_SCRATCH2);
        big_block_->ANDI2R(CPSR_REG, CPSR_REG, ~NZCV_FLAG_MASK, ALWAYS_SCRATCH2);

        big_block_->ORR(CPSR_REG, CPSR_REG, ALWAYS_SCRATCH1);
    }

    void visit_session::emit_cpsr_restore_nzcv() {
        big_block_->_MSR(true, false, CPSR_REG);
    }

    bool visit_session::emit_memory_access_chain(common::armgen::arm_reg base, reg_list guest_list, bool add,
        bool before, bool writeback, bool load) {
        std::uint8_t last_reg = 0;

        emit_cpsr_update_nzcv();
        std::uint8_t *lookup_route = big_block_->get_writeable_code_ptr();

        common::armgen::arm_reg backback = reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR);

        // Temporary LR
        big_block_->MOV(backback, 0);

        common::armgen::arm_reg guest_addr_reg = reg_supplier_.map(base, (writeback ? ALLOCATE_FLAG_DIRTY : 0));
        base = emit_address_lookup(guest_addr_reg, load);

        // Check if we should jump back
        big_block_->CMP(backback, 0);

        big_block_->set_cc(common::CC_NEQ);
        big_block_->B(backback);
        big_block_->set_cc(common::CC_AL);

        reg_supplier_.spill_lock_all(REG_SCRATCH_TYPE_GPR);

        bool is_first = true;

        // Map to register as much registers as possible, then flush them all to load/store
        while (last_reg <= 15) {
            bool full = false;

            while (last_reg <= 15) {
                if (common::bit(guest_list, last_reg)) {
                    const common::armgen::arm_reg orig = static_cast<common::armgen::arm_reg>(
                        common::armgen::R0 + last_reg);

                    common::armgen::arm_reg mapped = reg_supplier_.map(orig, (load) ? 0 : ALLOCATE_FLAG_DIRTY);

                    if (mapped == common::armgen::INVALID_REG) {
                        full = true;
                        break;
                    } else {
                        if (!is_first) {
                            // Check if the guest address now crosses a new page
                            big_block_->ANDI2R(ALWAYS_SCRATCH1, guest_addr_reg, CPAGE_MASK, ALWAYS_SCRATCH2);
                            big_block_->CMP(ALWAYS_SCRATCH1, 0);

                            auto done_refresh = big_block_->B_CC(common::CC_NEQ);

                            big_block_->CMP(base, 0);
                            auto done_refresh2 = big_block_->B_CC(common::CC_NEQ);

                            // Try to lookup the address again.
                            // Remember the PC. Note in ARM mode the PC is forward by 8 bytes.
                            // By doing this we just gonna jump to after the branch
                            big_block_->MOV(backback, common::armgen::R15);
                            big_block_->B(lookup_route);

                            big_block_->set_jump_target(done_refresh);
                            big_block_->set_jump_target(done_refresh2);
                        } else {
                            is_first = false;
                        }

                        big_block_->CMP(base, 0);
                        auto lookup_no_good = big_block_->B_CC(common::CC_EQ);

                        if (load) {
                            big_block_->LDR(mapped, base, 0);
                        } else {
                            big_block_->STR(mapped, base, 0);
                        }

                        big_block_->set_jump_target(lookup_no_good);
                        big_block_->MOV(common::armgen::R0, guest_addr_reg);
                        big_block_->MOV(ALWAYS_SCRATCH2, mapped);

                        reg_supplier_.flush_host_regs_for_host_call();

                        big_block_->MOV(common::armgen::R1, common::armgen::R0);
                        big_block_->MOV(common::armgen::R0, reinterpret_cast<std::uint32_t>(this));

                        if (load) {
                            big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_read_dword_router);

                            common::armgen::arm_reg new_mapped = reg_supplier_.map(orig, (load) ? 0 : ALLOCATE_FLAG_DIRTY);
                            big_block_->MOV(new_mapped, common::armgen::R0);

                            mapped = new_mapped;
                        } else {
                            big_block_->MOV(common::armgen::R2, ALWAYS_SCRATCH2);
                            big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_write_dword_router);
                        }

                        // Decrement or increment guest base and host base
                        if (add) {
                            big_block_->ADDI2R(base, base, 4, ALWAYS_SCRATCH1);
                            big_block_->ADDI2R(guest_addr_reg, guest_addr_reg, 4, ALWAYS_SCRATCH1);
                        } else {
                            big_block_->SUBI2R(base, base, 4, ALWAYS_SCRATCH1);
                            big_block_->SUBI2R(guest_addr_reg, guest_addr_reg, 4, ALWAYS_SCRATCH1);
                        }
                    }
                }

                last_reg++;
            }

            if (full)
                reg_supplier_.flush_all();
        }
        
        reg_supplier_.release_spill_lock_all(REG_SCRATCH_TYPE_GPR);
        reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);

        emit_cpsr_restore_nzcv();

        return true;
    }

    void visit_session::sync_registers() {
        reg_supplier_.flush_all();
        big_block_->emit_pc_flush(crr_block_->current_address());
    }

    bool visit_session::emit_undefined_instruction_handler() {
        sync_registers();

        big_block_->MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(big_block_));
        big_block_->MOVI2R(common::armgen::R1, exception_type_undefined_inst);
        big_block_->MOVI2R(common::armgen::R2, crr_block_->current_address());

        big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_raise_exception_router);
        return false;
    }

    bool visit_session::emit_system_call_handler(const std::uint32_t n) {
        sync_registers();

        big_block_->MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(big_block_));
        big_block_->MOVI2R(common::armgen::R1, n);

        big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_raise_system_call);
        return true;
    }
}