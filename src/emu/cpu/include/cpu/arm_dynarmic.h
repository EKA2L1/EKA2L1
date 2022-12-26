/*
 * Copyright (c) 2018 EKA2L1 Team.
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
#include <cpu/dyncom/arm_dyncom.h>

#include <dynarmic/interface/A32/a32.h>
#include <dynarmic/interface/A32/config.h>
#include <dynarmic/interface/exclusive_monitor.h>

#include <map>
#include <memory>

namespace eka2l1 {
    class ntimer;

    namespace arm {
        class dynarmic_core_callback;

        class dynarmic_exclusive_monitor : public exclusive_monitor {
        private:
            friend class dynarmic_core;
            Dynarmic::ExclusiveMonitor monitor_;

        public:
            explicit dynarmic_exclusive_monitor(const std::size_t processor_count);

            ~dynarmic_exclusive_monitor() override {
            }

            std::uint8_t exclusive_read8(core *cc, address vaddr) override;
            std::uint16_t exclusive_read16(core *cc, address vaddr) override;
            std::uint32_t exclusive_read32(core *cc, address vaddr) override;
            std::uint64_t exclusive_read64(core *cc, address vaddr) override;
            void clear_exclusive() override;

            bool exclusive_write8(core *cc, address vaddr, std::uint8_t value) override;
            bool exclusive_write16(core *cc, address vaddr, std::uint16_t value) override;
            bool exclusive_write32(core *cc, address vaddr, std::uint32_t value) override;
            bool exclusive_write64(core *cc, address vaddr, std::uint64_t value) override;
        };

        class dynarmic_core : public core {
            friend class dynarmic_core_callback;

            std::unique_ptr<Dynarmic::A32::Jit> jit;
            std::unique_ptr<dynarmic_core_callback> cb;

            arm::dyncom_core interpreter;
            Dynarmic::TLB<9> tlb_obj;

            std::uint32_t ticks_executed{ 0 };
            std::uint32_t ticks_target{ 0 };

            bool interpreter_callback_inited;

        public:
            explicit dynarmic_core(arm::exclusive_monitor *monitor);
            ~dynarmic_core() override;

            void run(const std::uint32_t instruction_count) override;
            void stop() override;

            void step() override;

            uint32_t get_reg(size_t idx) override;
            uint32_t get_sp() override;
            uint32_t get_pc() override;
            uint32_t get_vfp(size_t idx) override;

            void set_reg(size_t idx, uint32_t val) override;
            void set_pc(uint32_t val) override;
            void set_sp(uint32_t val) override;
            void set_lr(uint32_t val) override;
            void set_vfp(size_t idx, uint32_t val) override;

            uint32_t get_cpsr() override;
            uint32_t get_fpscr() override;
            uint32_t get_lr() override;
            void set_cpsr(uint32_t val) override;
            void set_fpscr(uint32_t val) override;

            void save_context(thread_context &ctx) override;
            void load_context(const thread_context &ctx) override;

            bool is_thumb_mode() override;

            void set_tlb_page(address vaddr, std::uint8_t *ptr, prot protection) override;
            void dirty_tlb_page(address addr) override;
            void flush_tlb() override;

            void clear_instruction_cache() override;

            void imb_range(address addr, std::size_t size) override;

            std::uint32_t get_num_instruction_executed() override;

            bool should_clear_old_memory_map() const override {
                return false;
            }
        };
    }
}
