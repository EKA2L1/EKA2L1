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

    static std::uint8_t dashixiong_read_byte_router(dashixiong_block *self, const vaddress addr) {
        return self->read_byte(addr);
    }

    static std::uint16_t dashixiong_read_word_router(dashixiong_block *self, const vaddress addr) {
        return self->read_word(addr);
    }

    static std::uint32_t dashixiong_read_dword_router(dashixiong_block *self, const vaddress addr) {
        return self->read_dword(addr);
    }

    static void dashixiong_write_byte_router(dashixiong_block *self, const vaddress addr, const std::uint8_t val) {
        self->write_byte(addr, val);
    }

    static void dashixiong_write_word_router(dashixiong_block *self, const vaddress addr, const std::uint16_t val) {
        self->write_word(addr, val);
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

    static void dashixiong_print_22debug(const std::uint32_t val) {
        LOG_TRACE(CPU_12L1R, "Print debug: 0x{:X}", val);
    }

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
        big_block_->ANDI2R(final_addr, base, ~CPAGE_MASK, scratch3);     // remove the unaligned
        big_block_->CMP(final_addr, ALWAYS_SCRATCH2);

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

        // This register must persits until this routine end.
        // It holds the guest register address.
        common::armgen::arm_reg guest_addr_reg = reg_supplier_.map(base, (writeback ? ALLOCATE_FLAG_DIRTY : 0));
        common::armgen::arm_reg base_guest_reg = base;

        // Spill lock this base guest reg
        reg_supplier_.spill_lock(base_guest_reg);

        auto emit_addr_modification = [&](const bool ignore_base) {
            // Decrement or increment guest base and host base
            if (add) {
                if (!ignore_base) {
                    big_block_->set_cc(common::CC_NEQ);
                    big_block_->ADDI2R(base, base, 4, ALWAYS_SCRATCH1);
                    big_block_->set_cc(common::CC_AL);
                }

                big_block_->ADDI2R(guest_addr_reg, guest_addr_reg, 4, ALWAYS_SCRATCH1);
            } else {
                if (!ignore_base) {
                    big_block_->set_cc(common::CC_NEQ);
                    big_block_->SUBI2R(base, base, 4, ALWAYS_SCRATCH1);
                    big_block_->set_cc(common::CC_AL);
                }

                big_block_->SUBI2R(guest_addr_reg, guest_addr_reg, 4, ALWAYS_SCRATCH1);
            }
        };

        // R1 is reserved for host call, but this backback is scratch LR, and it does not relate
        // to R1 used as arguments in anyway (branch differentiate)
        common::armgen::arm_reg backback = reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR);

        // Temporary LR
        big_block_->MOV(backback, 0);

        if (before) {
            emit_addr_modification(true);
        }

        std::uint8_t *lookup_route = big_block_->get_writeable_code_ptr();
        base = emit_address_lookup(guest_addr_reg, load);

        // Check if we should jump back
        big_block_->CMP(backback, 0);

        big_block_->set_cc(common::CC_NEQ);
        big_block_->B(backback);
        big_block_->set_cc(common::CC_AL);

        bool is_first = true;
        bool block_cont = true;

        // Map to register as much registers as possible, then flush them all to load/store
        while (last_reg <= 15) {
            if (common::bit(guest_list, last_reg)) {
                const common::armgen::arm_reg orig = static_cast<common::armgen::arm_reg>(
                    common::armgen::R0 + last_reg);

                common::armgen::arm_reg mapped = common::armgen::INVALID_REG;

                if (last_reg != 15)
                    mapped = reg_supplier_.map(orig, (load) ? 0 : ALLOCATE_FLAG_DIRTY);

                if (mapped == common::armgen::INVALID_REG) {
                    LOG_ERROR(CPU_12L1R, "Can't map another register for some reason...");
                    assert(false);
                } else {
                    if (!is_first) {
                        if (before) {
                            emit_addr_modification(false);
                        }

                        big_block_->CMP(base, 0);
                        auto base_need_lookup = big_block_->B_CC(common::CC_EQ);

                        // Check if the guest address now crosses a new page
                        big_block_->ANDI2R(ALWAYS_SCRATCH1, guest_addr_reg, CPAGE_MASK, ALWAYS_SCRATCH2);
                        big_block_->CMP(ALWAYS_SCRATCH1, 0);

                        auto done_refresh = big_block_->B_CC(common::CC_NEQ);
                        big_block_->set_jump_target(base_need_lookup);

                        // Try to lookup the address again.
                        // Remember the PC. Note in ARM mode the PC is forward by 8 bytes.
                        // By doing this we just gonna jump to after the branch
                        big_block_->MOV(backback, common::armgen::R15);
                        big_block_->B(lookup_route);

                        big_block_->set_jump_target(done_refresh);
                    } else {
                        is_first = false;
                    }

                    big_block_->CMP(base, 0);
                    auto lookup_good = big_block_->B_CC(common::CC_NEQ);

                    big_block_->MOV(common::armgen::R0, guest_addr_reg);

                    big_block_->PUSH(3, common::armgen::R1, common::armgen::R2, common::armgen::R3);

                    big_block_->MOV(common::armgen::R1, common::armgen::R0);
                    big_block_->MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(big_block_));

                    if (load) {
                        big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_read_dword_router);
                        big_block_->POP(3, common::armgen::R1, common::armgen::R2, common::armgen::R3);

                        if (last_reg == 15) {
                            // Store to lastest PC
                            link_block_ambiguous(common::armgen::R0);
                            block_cont = false;
                        } else {
                            big_block_->MOV(mapped, common::armgen::R0);
                        }
                    } else {
                        if (last_reg == 15) {
                            big_block_->MOVI2R(common::armgen::R2, crr_block_->current_address()
                                + (crr_block_->thumb_ ? 4 : 8), ALWAYS_SCRATCH2);
                        } else {
                            big_block_->MOV(common::armgen::R2, mapped);
                        }

                        big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_write_dword_router);
                        big_block_->POP(3, common::armgen::R1, common::armgen::R2, common::armgen::R3);
                    }

                    auto add_up_value_br = big_block_->B();
                    big_block_->set_jump_target(lookup_good);

                    if (load) {
                        if (last_reg == 15) {
                            big_block_->LDR(ALWAYS_SCRATCH1, base, 0);
                            link_block_ambiguous(ALWAYS_SCRATCH1);

                            block_cont = false;
                        } else {
                            big_block_->LDR(mapped, base, 0);
                        }
                    } else {
                        if (last_reg == 15) {
                            big_block_->MOVI2R(ALWAYS_SCRATCH1, crr_block_->current_address()
                                    + (crr_block_->thumb_ ? 4 : 8), ALWAYS_SCRATCH2);

                            big_block_->STR(ALWAYS_SCRATCH1, base, 0);
                        } else {
                            big_block_->STR(mapped, base, 0);
                        }
                    }

                    big_block_->set_jump_target(add_up_value_br);
                    big_block_->CMP(base, 0);

                    if (!before) {
                        emit_addr_modification(false);
                    }
                }
            }

            last_reg++;
        }

        reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);
        reg_supplier_.release_spill_lock(base_guest_reg);

        if (!writeback) {
            // Flush it xD. Its value is trashed now, so we don't want to keep it around and
            // being misunderstood. Why its trashed? Because we write subtraction/addition to it.
            reg_supplier_.flush(base_guest_reg);
        }

        emit_cpsr_restore_nzcv();
        return block_cont;
    }

    bool visit_session::emit_memory_access(common::armgen::arm_reg target_mapped, common::armgen::arm_reg base_mapped,
        common::armgen::operand2 op2, const std::uint8_t bit_count, bool is_signed, bool add, bool pre_index, bool writeback, bool read) {
        emit_cpsr_update_nzcv();

        common::armgen::arm_reg host_base_addr = emit_address_lookup(base_mapped, read);
        if (host_base_addr == common::armgen::INVALID_REG) {
            LOG_ERROR(CPU_12L1R, "Failed to get host base address register");
            return false;
        }

        big_block_->CMP(host_base_addr, 0);
        auto lookup_good = big_block_->B_CC(common::CC_NEQ);

        // Here we fallback to old friend. hmmmm
        // Calculate the address
        big_block_->PUSH(3, common::armgen::R1, common::armgen::R2, common::armgen::R3);

        if (!read) {
            big_block_->MOV(common::armgen::R2, target_mapped);
        }

        if (base_mapped != common::armgen::R1)
            big_block_->MOV(common::armgen::R1, base_mapped);

        if (pre_index) {
            if (add)
                big_block_->ADD(common::armgen::R1, common::armgen::R1, op2);
            else
                big_block_->SUB(common::armgen::R1, common::armgen::R1, op2);
        }

        big_block_->PUSH(1, common::armgen::R1);
        big_block_->MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(big_block_));

        if (read) {
            switch (bit_count) {
                case 8:
                    big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_read_byte_router);
                    break;

                case 16:
                    big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_read_word_router);
                    break;

                case 32:
                    big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_read_dword_router);
                    break;

                default:
                    assert(false);
                    break;
            }
        } else {
            switch (bit_count) {
                case 8:
                    big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_write_byte_router);
                    break;

                case 16:
                    big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_write_word_router);
                    break;

                case 32:
                    big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_write_dword_router);
                    break;

                default:
                    assert(false);
                    break;
            }
        }

        big_block_->POP(1, common::armgen::R1);

        if (!pre_index) {
            if (add)
                big_block_->ADD(common::armgen::R1, common::armgen::R1, op2);
            else
                big_block_->SUB(common::armgen::R1, common::armgen::R1, op2);
        }

        if (writeback) {
            big_block_->MOV(ALWAYS_SCRATCH2, common::armgen::R1);
        }

        big_block_->POP(3, common::armgen::R1, common::armgen::R2, common::armgen::R3);

        if (writeback) {
            big_block_->MOV(base_mapped, ALWAYS_SCRATCH2);
        }

        if (read) {
            if (is_signed) {
                switch (bit_count) {
                    case 8:
                        big_block_->SXTB(target_mapped, common::armgen::R0);
                        break;

                    case 16:
                        big_block_->SXTH(target_mapped, common::armgen::R0);
                        break;

                    default:
                        assert(false);
                        break;
                }
            } else {
                big_block_->MOV(target_mapped, common::armgen::R0);
            }
        }

        auto done_all = big_block_->B();
        big_block_->set_jump_target(lookup_good);

        if (read) {
            switch (bit_count) {
                case 8:
                    if (is_signed)
                        big_block_->LDRSB(target_mapped, host_base_addr, op2, add, pre_index, writeback);
                    else
                        big_block_->LDRB(target_mapped, host_base_addr, op2, add, pre_index, writeback);

                    break;

                case 16:
                    if (is_signed)
                        big_block_->LDRSH(target_mapped, host_base_addr, op2, add, pre_index, writeback);
                    else
                        big_block_->LDRH(target_mapped, host_base_addr, op2, add, pre_index, writeback);

                    break;

                case 32:
                    big_block_->LDRSB(target_mapped, host_base_addr, op2, add, pre_index, writeback);
                    break;

                default:
                    assert(false);
                    break;
            }
        } else {
            switch (bit_count) {
                case 8:
                    big_block_->STRB(target_mapped, host_base_addr, op2, add, pre_index, writeback);
                    break;

                case 16:
                    big_block_->STRH(target_mapped, host_base_addr, op2, add, pre_index, writeback);
                    break;

                case 32:
                    big_block_->STR(target_mapped, host_base_addr, op2, add, pre_index, writeback);
                    break;

                default:
                    assert(false);
                    break;
            }
        }

        big_block_->set_jump_target(done_all);
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

    void visit_session::link_block_ambiguous(common::armgen::arm_reg new_pc_value) {
        // TODO: Exchange mode
        big_block_->STR(new_pc_value, CORE_STATE_REG, offsetof(core_state, gprs_[15]));
        crr_block_->link_type_ = TRANSLATED_BLOCK_LINK_AMBIGUOUS;
    }
}