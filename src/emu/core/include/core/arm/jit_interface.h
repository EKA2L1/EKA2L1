/*
 * Copyright (c) 2018 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project 
 * (see bentokun.github.com/EKA2L1).
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

#include <array>
#include <common/types.h>

namespace eka2l1 {
    namespace arm {
        // Allow multiple backends to be added
        class jit_interface {
        public:
            // yuzu
            struct thread_context {
                std::array<uint32_t, 31> cpu_registers;
                uint64_t sp;
                uint64_t pc;
                uint64_t cpsr;
                std::array<uint64_t, 32> fpu_registers;
                uint64_t fpscr;
            };

            virtual void run() = 0;
            virtual void stop() = 0;
            virtual bool execute_instructions(int num_instructions) = 0;

            virtual void step() = 0;

            virtual uint32_t get_reg(size_t idx) = 0;
            virtual uint64_t get_sp() = 0;
            virtual uint64_t get_pc() = 0;
            virtual uint64_t get_vfp(size_t idx) = 0;

            virtual void set_reg(size_t idx, uint32_t val) = 0;
            virtual void set_pc(uint64_t val) = 0;
            virtual void set_lr(uint64_t val) = 0;
            virtual void set_sp(uint32_t val) = 0;
            virtual void set_vfp(size_t idx, uint64_t val) = 0;
            virtual void set_entry_point(address ep) = 0;
            virtual address get_entry_point() = 0;
            virtual uint32_t get_cpsr() = 0;

            virtual void save_context(thread_context &ctx) = 0;
            virtual void load_context(const thread_context &ctx) = 0;

            virtual void set_stack_top(address addr) = 0;
            virtual address get_stack_top() = 0;

            virtual void prepare_rescheduling() = 0;

            virtual bool is_thumb_mode() = 0;
        };
    }
}

