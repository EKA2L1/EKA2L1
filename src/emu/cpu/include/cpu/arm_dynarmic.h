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

#include <dynarmic/A32/a32.h>
#include <dynarmic/A32/config.h>

#include <map>
#include <memory>

namespace eka2l1 {
    class ntimer;

    namespace arm {
        class dynarmic_core_callback;

        class dynarmic_core : public core {
            friend class dynarmic_core_callback;

            std::unique_ptr<Dynarmic::A32::Jit> jit;
            std::unique_ptr<dynarmic_core_callback> cb;

            std::array<std::uint8_t *, Dynarmic::A32::UserConfig::NUM_PAGE_TABLE_ENTRIES>
                page_dyn;

            std::uint32_t ticks_executed{ 0 };
            std::uint32_t ticks_target{ 0 };

        public:
            explicit dynarmic_core();
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
            uint32_t get_lr() override;
            void set_cpsr(uint32_t val) override;

            void save_context(thread_context &ctx) override;
            void load_context(const thread_context &ctx) override;

            void set_entry_point(address ep) override;
            address get_entry_point() override;

            void set_stack_top(address addr) override;
            address get_stack_top() override;

            void prepare_rescheduling() override;

            bool is_thumb_mode() override;

            void page_table_changed() override;

            void map_backing_mem(address vaddr, size_t size, uint8_t *ptr, prot protection) override;

            void unmap_memory(address addr, size_t size) override;

            void clear_instruction_cache() override;

            void imb_range(address addr, std::size_t size) override;

            std::uint32_t get_num_instruction_executed() override;

            bool should_clear_old_memory_map() const override {
                return false;
            }

            void set_asid(std::uint8_t num) override;
            std::uint8_t get_asid() const override;
            std::uint8_t get_max_asid_available() const override;
        };
    }
}
