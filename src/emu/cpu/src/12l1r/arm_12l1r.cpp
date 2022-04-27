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

#include <common/log.h>
#include <cpu/12l1r/arm_12l1r.h>

namespace eka2l1::arm {
    r12l1_core::r12l1_core(arm::exclusive_monitor *monitor, const std::size_t page_bits)
        : mem_cache_(page_bits)
        , big_block_(nullptr)
        , monitor_(reinterpret_cast<arm::r12l1::exclusive_monitor *>(monitor)) {
        // Set the state's TLB entries
        jit_state_.entries_ = mem_cache_.entries;
        big_block_ = std::make_unique<r12l1::dashixiong_block>(this);
    }

    r12l1_core::~r12l1_core() {
    }

    void r12l1_core::run(const std::uint32_t instruction_count) {
        if (!jit_state_.should_break_) {
            LOG_ERROR(CPU_12L1R, "Requesting a run while the recompiler is running!");
            return;
        }

        // Set the number of instruction that's targeted to run
        target_ticks_run_ = instruction_count;
        jit_state_.ticks_left_ = target_ticks_run_;
        jit_state_.should_break_ = false;

        big_block_->enter_dispatch(&jit_state_);

        // Set it again
        jit_state_.should_break_ = true;
    }

    void r12l1_core::stop() {
        jit_state_.should_break_ = true;
    }

    void r12l1_core::step() {
        // TODO: This step is emulated! So it is not accurate.
        run(1);
    }

    std::uint32_t r12l1_core::get_reg(size_t idx) {
        return jit_state_.gprs_[idx];
    }

    std::uint32_t r12l1_core::get_sp() {
        return jit_state_.gprs_[13];
    }

    std::uint32_t r12l1_core::get_pc() {
        return jit_state_.gprs_[15];
    }

    std::uint32_t r12l1_core::get_vfp(size_t idx) {
        return jit_state_.fprs_[idx];
    }

    void r12l1_core::set_reg(size_t idx, std::uint32_t val) {
        jit_state_.gprs_[idx] = val;
    }

    void r12l1_core::set_cpsr(uint32_t val) {
        jit_state_.cpsr_ = val;
    }

    void r12l1_core::set_pc(uint32_t val) {
        jit_state_.gprs_[15] = val;
    }

    void r12l1_core::set_lr(uint32_t val) {
        jit_state_.gprs_[14] = val;
    }

    void r12l1_core::set_sp(uint32_t val) {
        jit_state_.gprs_[13] = val;
    }

    void r12l1_core::set_vfp(size_t idx, uint32_t val) {
        jit_state_.fprs_[15] = val;
    }

    std::uint32_t r12l1_core::get_lr() {
        return jit_state_.gprs_[14];
    }

    std::uint32_t r12l1_core::get_cpsr() {
        return jit_state_.cpsr_;
    }

    std::uint32_t r12l1_core::get_fpscr() {
        return jit_state_.fpscr_;
    }

    void r12l1_core::save_context(thread_context &ctx) {
        for (std::size_t i = 0; i < 16; i++) {
            ctx.cpu_registers[i] = jit_state_.gprs_[i];
        }

        for (std::size_t i = 0; i < 64; i++) {
            ctx.fpu_registers[i] = jit_state_.fprs_[i];
        }

        ctx.cpsr = jit_state_.cpsr_;
        ctx.fpscr = jit_state_.fpscr_;
        ctx.wrwr = jit_state_.wrwr_;
    }

    void r12l1_core::load_context(const thread_context &ctx) {
        for (std::size_t i = 0; i < 16; i++) {
            jit_state_.gprs_[i] = ctx.cpu_registers[i];
        }

        for (std::size_t i = 0; i < 64; i++) {
            jit_state_.fprs_[i] = ctx.fpu_registers[i];
        }

        jit_state_.cpsr_ = ctx.cpsr;
        jit_state_.fpscr_ = ctx.fpscr;
        jit_state_.wrwr_ = ctx.wrwr;

#if R12L1_ENABLE_FUZZ
        if (big_block_->config_flags() & r12l1::dashixiong_block::FLAG_ENABLE_FUZZ) {
            big_block_->fuzz_end();
            big_block_->fuzz_start();
        }
#endif
    }

    bool r12l1_core::is_thumb_mode() {
        return jit_state_.cpsr_ & 0x20;
    }

    void r12l1_core::set_tlb_page(address vaddr, std::uint8_t *ptr, prot protection) {
        mem_cache_.add(vaddr, ptr, protection);
    }

    void r12l1_core::dirty_tlb_page(address addr) {
        mem_cache_.make_dirty(addr);
    }

    void r12l1_core::flush_tlb() {
        mem_cache_.flush();
    }

    void r12l1_core::clear_instruction_cache() {
        big_block_->flush_all();
    }

    void r12l1_core::imb_range(address addr, std::size_t size) {
        big_block_->flush_range(addr, static_cast<vaddress>(addr + size));
    }

    std::uint32_t r12l1_core::get_num_instruction_executed() {
        return target_ticks_run_ - jit_state_.ticks_left_;
    }
}