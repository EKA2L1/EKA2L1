/*
 * Copyright (c) 2021 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
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

#include <cpu/dyncom/arm_dyncom.h>
#include <cpu/dyncom/arm_dyncom_interpreter.h>
#include <cpu/dyncom/arm_dyncom_trans.h>

#include <common/log.h>

namespace eka2l1::arm {
    dyncom_core::dyncom_core(arm::exclusive_monitor *monitor, const std::size_t page_bits)
        : monitor_(monitor)
        , state_(nullptr)
        , ticks_executed_(0)
        , mem_cache_(page_bits) {
        state_ = std::make_unique<ARMul_State>(this, USER32MODE);
    }

    dyncom_core::~dyncom_core() {
    }

    void dyncom_core::run(const std::uint32_t instruction_count) {
        ticks_executed_ = 0;
        state_->NumInstrsToExecute = instruction_count;

        InterpreterMainLoop(state_.get(), ticks_executed_);
    }

    void dyncom_core::stop() {
        state_->NumInstrsToExecute = 0;
    }

    void dyncom_core::step() {
        ticks_executed_ = 0;
        state_->NumInstrsToExecute = 1;

        InterpreterMainLoop(state_.get(), ticks_executed_);
    }

    std::uint32_t dyncom_core::get_reg(size_t idx) {
        return state_->Reg[idx];
    }

    std::uint32_t dyncom_core::get_sp() {
        return state_->Reg[13];
    }

    uint32_t dyncom_core::get_pc() {
        return state_->Reg[15];
    }

    uint32_t dyncom_core::get_vfp(size_t idx) {
        return state_->ExtReg[idx];
    }

    void dyncom_core::set_reg(size_t idx, uint32_t val) {
        state_->Reg[idx] = val;
    }

    void dyncom_core::set_pc(uint32_t val) {
        state_->Reg[15] = val;
    }

    void dyncom_core::set_sp(uint32_t val) {
        state_->Reg[13] = val;
    }

    void dyncom_core::set_lr(uint32_t val) {
        state_->Reg[14] = val;
    }

    void dyncom_core::set_vfp(size_t idx, uint32_t val) {
        state_->ExtReg[idx] = val;
    }

    uint32_t dyncom_core::get_cpsr() {
        return state_->Cpsr;
    }

    uint32_t dyncom_core::get_lr() {
        return state_->Reg[14];
    }

    void dyncom_core::set_cpsr(uint32_t val) {
        state_->Cpsr = val;
    }

    bool dyncom_core::is_thumb_mode() {
        return get_cpsr() & 0x20;
    }

    void dyncom_core::save_context(thread_context &ctx) {
        ctx.cpsr = state_->Cpsr;

        for (uint8_t i = 0; i < 16; i++) {
            ctx.cpu_registers[i] = get_reg(i);
        }
    }

    void dyncom_core::load_context(const thread_context &ctx) {
        clear_instruction_cache();

        for (uint8_t i = 0; i < 16; i++) {
            state_->Reg[i] = ctx.cpu_registers[i];
        }

        set_cpsr(ctx.cpsr);
    }

    void dyncom_core::set_tlb_page(address vaddr, std::uint8_t *ptr, prot protection) {
        mem_cache_.add(vaddr, ptr, protection);
    }

    void dyncom_core::dirty_tlb_page(address addr) {
        mem_cache_.make_dirty(addr);
    }

    void dyncom_core::flush_tlb() {
        mem_cache_.flush();
    }

    void dyncom_core::clear_instruction_cache() {
        state_->instruction_cache.clear();
        state_->trans_cache_buf_top = 0;
    }

    void dyncom_core::imb_range(address addr, std::size_t size) {
        clear_instruction_cache();
    }

    std::uint32_t dyncom_core::get_num_instruction_executed() {
        return ticks_executed_;
    }

    void dyncom_core::set_asid(std::uint8_t num) {

    }

    std::uint8_t dyncom_core::get_asid() const {
        return 0;
    }

    std::uint8_t dyncom_core::get_max_asid_available() const {
        return 0;
    }
}