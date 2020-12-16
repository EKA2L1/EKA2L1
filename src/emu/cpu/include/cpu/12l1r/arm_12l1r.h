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

#include <cpu/arm_interface.h>
#include <common/armemitter.h>

#include <cpu/12l1r/tlb.h>
#include <cpu/12l1r/core_state.h>
#include <cpu/12l1r/block_gen.h>

namespace eka2l1::arm {
    class r12l1_core: public core {
    private:
        r12l1::core_state jit_state_;
        r12l1::dashixiong_block big_block_;
        r12l1::tlb mem_cache_;

        arm::exclusive_monitor *monitor_;
        std::uint32_t target_ticks_run_;
 
    public:
        explicit r12l1_core(arm::exclusive_monitor *monitor, const std::size_t page_bits);
        ~r12l1_core() override;

        void run(const std::uint32_t instruction_count) override;
        void stop() override;
        void step() override;

        std::uint32_t get_reg(size_t idx) override;
        std::uint32_t get_sp() override;
        std::uint32_t get_pc() override;
        std::uint32_t get_vfp(size_t idx) override;

        void set_reg(size_t idx, std::uint32_t val) override;

        void set_cpsr(uint32_t val) override;
        void set_pc(uint32_t val) override;
        void set_lr(uint32_t val) override;

        void set_sp(uint32_t val) override;
        void set_vfp(size_t idx, uint32_t val) override;
        uint32_t get_lr() override;
        
        uint32_t get_cpsr() override;

        void save_context(thread_context &ctx) override;
        void load_context(const thread_context &ctx) override;

        bool is_thumb_mode() override;

        void set_tlb_page(address vaddr, std::uint8_t *ptr, prot protection) override;
        void dirty_tlb_page(address addr) override;
        void flush_tlb() override;

        void clear_instruction_cache() override;
        void imb_range(address addr, std::size_t size) override;

        bool should_clear_old_memory_map() const {
            return true;
        }

        void set_asid(std::uint8_t num) override;
        std::uint8_t get_asid() const override;
        std::uint8_t get_max_asid_available() const override;

        std::uint32_t get_num_instruction_executed() override;
    };
}