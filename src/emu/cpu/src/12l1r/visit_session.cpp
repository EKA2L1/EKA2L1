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
        : flag_(common::CC_NV)
        , cond_modified_(false)
        , cpsr_ever_updated_(false)
        , cond_failed_(false)
        , last_inst_count_(0)
        , big_block_(bro)
        , crr_block_(crr)
        , reg_supplier_(bro) {
    }

    bool visit_session::condition_passed(common::cc_flags cc, const bool force_end_last) {
        if (force_end_last && (flag_ != common::CC_NV)) {
            reg_supplier_.flush_all();

            if (flag_ != common::CC_AL) {
                big_block_->emit_cycles_count_add(crr_block_->inst_count_ - last_inst_count_);
                big_block_->set_jump_target(end_target_);

                last_inst_count_ = crr_block_->inst_count_;
            }

            flag_ = common::CC_NV;
        }

        if (flag_ == common::CC_NV) {
            flag_ = cc;

            // Emit jump to next block if condition not satisfied.
            if (flag_ != common::CC_AL)
                end_target_ = big_block_->B_CC(common::invert_cond(cc));

            return true;
        }

        if ((cond_modified_ && (flag_ != common::CC_AL)) || (cc != flag_)) {
            cond_failed_ = true;
            return false;
        }

        return true;
    }

    // Should we hardcode this??? Hmmm
    static constexpr std::uint32_t CPAGE_BITS = 12;
    static constexpr std::uint32_t CPAGE_SIZE = 1 << CPAGE_BITS;
    static constexpr std::uint32_t CPAGE_MASK = CPAGE_SIZE - 1;

    void dashixiong_print_22debug(const std::uint32_t val) {
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
        reg_supplier_.done_scratching_this(scratch3);

        return final_addr;
    }

    static constexpr std::uint32_t NZCVQ_FLAG_MASK = 0b11111 << 27;

    void visit_session::emit_cpsr_update_nzcvq() {
        emit_cpsr_update_sel(true);
    }

    void visit_session::emit_cpsr_update_sel(const bool nzcvq) {
        big_block_->MRS(ALWAYS_SCRATCH1);

        std::uint32_t mask = 0;
        if (nzcvq) {
            mask |= NZCVQ_FLAG_MASK;
        }

        big_block_->ANDI2R(ALWAYS_SCRATCH1, ALWAYS_SCRATCH1, mask, ALWAYS_SCRATCH2);
        big_block_->ANDI2R(CPSR_REG, CPSR_REG, ~mask, ALWAYS_SCRATCH2);
        big_block_->ORR(CPSR_REG, CPSR_REG, ALWAYS_SCRATCH1);

        cpsr_ever_updated_ = false;
    }

    void visit_session::emit_cpsr_restore_nzcvq() {
        emit_cpsr_restore_sel(true);
    }

    void visit_session::emit_cpsr_restore_sel(const bool nzcvq) {
        big_block_->_MSR(nzcvq, false, CPSR_REG);
    }

    void visit_session::emit_pc_write_exchange(common::armgen::arm_reg reg) {
        if (cpsr_ever_updated_)
            emit_cpsr_update_nzcvq();

        big_block_->emit_pc_write_exchange(reg);
    }

    void visit_session::emit_pc_write(common::armgen::arm_reg reg) {
        if (cpsr_ever_updated_)
            emit_cpsr_update_nzcvq();

        big_block_->emit_pc_write(reg);
    }

    void visit_session::emit_alu_jump(common::armgen::arm_reg reg) {
        // TODO: For armv7 and upper it also exchanges when PC is being written,  so might have
        //  to add some checks here...
        emit_pc_write(reg);
        emit_return_to_dispatch();
    }

    bool visit_session::emit_memory_access_chain(common::armgen::arm_reg base, reg_list guest_list, bool add,
        bool before, bool writeback, bool load) {
        std::int8_t last_reg = 0;
        emit_cpsr_update_nzcvq();

        // This register must persits until this routine end.
        // It holds the guest register address.
        common::armgen::arm_reg guest_addr_reg = reg_supplier_.map(base, (writeback ? ALLOCATE_FLAG_DIRTY : 0));
        if (!writeback) {
            common::armgen::arm_reg scratching = reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR);
            big_block_->MOV(scratching, guest_addr_reg);

            guest_addr_reg = scratching;
        }

        common::armgen::arm_reg base_guest_reg = base;

        // Spill lock this base guest reg
        if (writeback)
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

        // Registers are stored on the stack in numerical order, with the lowest numbered register at the lowest address
        // This is according to the manual of ARM :P So when it's decrement mode, we iterates from bottom to top.
        if (add) {
            last_reg = 0;
        } else {
            last_reg = 15;
        }

        // Map to register as much registers as possible, then flush them all to load/store
        while (add ? (last_reg <= 15) : (last_reg >= 0)) {
            if (common::bit(guest_list, last_reg)) {
                const common::armgen::arm_reg orig = static_cast<common::armgen::arm_reg>(
                    common::armgen::R0 + last_reg);

                common::armgen::arm_reg mapped = common::armgen::INVALID_REG;

                if (last_reg != 15)
                    mapped = reg_supplier_.map(orig, (load ? ALLOCATE_FLAG_DIRTY : 0));

                if ((last_reg != 15) && (mapped == common::armgen::INVALID_REG)) {
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
                            big_block_->emit_pc_write_exchange(common::armgen::R0);
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
                            big_block_->emit_pc_write_exchange(ALWAYS_SCRATCH1);

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

            if (add)
                last_reg++;
            else
                last_reg--;
        }

        reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);

        if (writeback)
            reg_supplier_.release_spill_lock(base_guest_reg);

        emit_cpsr_restore_nzcvq();

        if (!block_cont)
            emit_return_to_dispatch();

        return block_cont;
    }

    bool visit_session::emit_memory_access(common::armgen::arm_reg target, common::armgen::arm_reg base,
        common::armgen::operand2 op2, const std::uint8_t bit_count, bool is_signed, bool add,
        bool pre_index, bool writeback, bool read) {
        emit_cpsr_update_nzcvq();

        if (!writeback) {
            // Post index can also cause a writeback, so we might check that as well
            writeback = !pre_index;
        }

        common::armgen::arm_reg base_mapped = common::armgen::INVALID_REG;

        bool base_spill_locked = false;

        if (base == common::armgen::R15) {
            // Calculate address right away
            vaddress addr_to_base = common::align(crr_block_->current_address(), 4, 0) +
                    (crr_block_->thumb_ ? 4 : 8) + (add ? 1 : -1) * op2.Imm12();

            base_mapped = reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR);
            big_block_->MOVI2R(base_mapped, addr_to_base);

            // Empty op2 to 0
            op2 = 0;
        } else {
            // Map it as an normal register, and spill lock it
            base_mapped = reg_supplier_.map(base, writeback ? ALLOCATE_FLAG_DIRTY : 0);
            reg_supplier_.spill_lock(base);

            base_spill_locked = true;
        }

        if ((!writeback) && (base != common::armgen::R15)) {
            common::armgen::arm_reg scratch_base = reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR);
            big_block_->MOV(scratch_base, base_mapped);

            base_mapped = scratch_base;

            // Spill whether you want we are done! :DDD
            reg_supplier_.release_spill_lock(base);
            base_spill_locked = false;
        }

        if (pre_index && (op2.get_data() != 0)) {
            if (add)
                big_block_->ADD(base_mapped, base_mapped, op2);
            else
                big_block_->SUB(base_mapped, base_mapped, op2);
        }

        common::armgen::arm_reg host_base_addr = emit_address_lookup(base_mapped, read);
        if (host_base_addr == common::armgen::INVALID_REG) {
            LOG_ERROR(CPU_12L1R, "Failed to get host base address register");
            return false;
        }

        // Map the target here, no register can disturb us :D
        // CPSR should not be ruined
        common::armgen::arm_reg target_mapped = common::armgen::INVALID_REG;
        if (target == common::armgen::R15) {
            target_mapped = reg_supplier_.scratch(REG_SCRATCH_TYPE_GPR);
            if (!read) {
                // We are writing the PC, so load the current address into it
                big_block_->MOVI2R(target_mapped, crr_block_->current_address() + (crr_block_->thumb_ ? 4 : 8));
            }
        } else {
            // Normal mapping, no need to spill lock since no one can touch us here.
            target_mapped = reg_supplier_.map(target, read ? ALLOCATE_FLAG_DIRTY : 0);
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

        big_block_->POP(3, common::armgen::R1, common::armgen::R2, common::armgen::R3);

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
                        big_block_->LDRSB(target_mapped, host_base_addr, 0, true, true, false);
                    else
                        big_block_->LDRB(target_mapped, host_base_addr, 0, true, true, false);

                    break;

                case 16:
                    if (is_signed)
                        big_block_->LDRSH(target_mapped, host_base_addr, 0, true, true, false);
                    else
                        big_block_->LDRH(target_mapped, host_base_addr, 0, true, true, false);

                    break;

                case 32:
                    big_block_->LDR(target_mapped, host_base_addr, 0, true, true, false);
                    break;

                default:
                    assert(false);
                    break;
            }
        } else {
            switch (bit_count) {
                case 8:
                    big_block_->STRB(target_mapped, host_base_addr, 0, true, true, false);
                    break;

                case 16:
                    big_block_->STRH(target_mapped, host_base_addr, 0, true, true, false);
                    break;

                case 32:
                    big_block_->STR(target_mapped, host_base_addr, 0, true, true, false);
                    break;

                default:
                    assert(false);
                    break;
            }
        }

        big_block_->set_jump_target(done_all);

        if (!pre_index && (op2.get_data() != 0)) {
            // Increase mapped base
            if (add)
                big_block_->ADD(base_mapped, base_mapped, op2);
            else
                big_block_->SUB(base_mapped, base_mapped, op2);
        }

        emit_cpsr_restore_nzcvq();

        bool block_cont = true;

        if (read && (target == common::armgen::R15)) {
            // Link the block
            // Write the result
            emit_pc_write_exchange(target_mapped);
            emit_return_to_dispatch();

            block_cont = false;
        }

        if (base_spill_locked) {
            reg_supplier_.release_spill_lock(base);
        }

        reg_supplier_.done_scratching(REG_SCRATCH_TYPE_GPR);
        return block_cont;
    }

    void visit_session::sync_state() {
        big_block_->emit_cpsr_save();
        big_block_->emit_cycles_count_save();
        big_block_->emit_pc_flush(crr_block_->current_address());

        reg_supplier_.flush_all();
    }

    bool visit_session::emit_undefined_instruction_handler() {
        emit_cpsr_update_nzcvq();
        sync_state();

        big_block_->MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(big_block_));
        big_block_->MOVI2R(common::armgen::R1, exception_type_undefined_inst);
        big_block_->MOVI2R(common::armgen::R2, crr_block_->current_address());

        big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_raise_exception_router);

        emit_cpsr_restore_nzcvq();
        emit_return_to_dispatch(false);

        return false;
    }

    bool visit_session::emit_system_call_handler(const std::uint32_t n) {
        emit_cpsr_update_nzcvq();
        sync_state();

        big_block_->emit_pc_flush(crr_block_->current_address() + (crr_block_->thumb_ ? 2 : 4));

        big_block_->MOVI2R(common::armgen::R0, reinterpret_cast<std::uint32_t>(big_block_));
        big_block_->MOVI2R(common::armgen::R1, n);

        big_block_->quick_call_function(ALWAYS_SCRATCH2, dashixiong_raise_system_call);

        emit_cpsr_restore_nzcvq();

        // Don't save CPSR, we already update it and it may got modified in the interrupt
        emit_return_to_dispatch(false);

        return false;
    }

    void visit_session::emit_direct_link(const vaddress addr, const bool cpsr_save) {
        // Flush all registers in this
        reg_supplier_.flush_all();

        if (cpsr_ever_updated_) {
            emit_cpsr_update_nzcvq();
        }

        if (cpsr_save) {
            big_block_->emit_cpsr_save();
        }

        big_block_->emit_cycles_count_add(crr_block_->inst_count_ - last_inst_count_);

        block_link &link = crr_block_->get_or_add_link(addr);
        link.needs_.push_back(big_block_->B());
    }

    void visit_session::emit_return_to_dispatch(const bool cpsr_save) {
        // Flush all registers in this
        reg_supplier_.flush_all();

        if (cpsr_save) {
            big_block_->emit_cpsr_save();
        }

        big_block_->emit_cycles_count_add(crr_block_->inst_count_ - last_inst_count_);
        big_block_->emit_return_to_dispatch(crr_block_);
    }

    void visit_session::finalize() {
        big_block_->set_cc(common::CC_AL);
        reg_supplier_.flush_all();

        const bool no_offered_link = (crr_block_->links_.empty());

        if (flag_ != common::CC_AL) {
            big_block_->emit_cycles_count_add(crr_block_->inst_count_ - last_inst_count_);
            big_block_->set_jump_target(end_target_);
        }

        if ((flag_ != common::CC_AL) || no_offered_link) {
            if (cpsr_ever_updated_) {
                emit_cpsr_update_nzcvq();
                big_block_->emit_cpsr_save();
            }

            // Add branching to next block, making it highest priority
            crr_block_->get_or_add_link(crr_block_->current_address() - (cond_failed_ ?
                crr_block_->last_inst_size_ : 0), 0);
        }

        // Emit links
        big_block_->emit_block_links(crr_block_);

        big_block_->flush_lit_pool();
        big_block_->edit_block_links(crr_block_, false);

        crr_block_->translated_size_ = big_block_->get_code_pointer() - crr_block_->translated_code_;
    }
}