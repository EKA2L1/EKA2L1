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

#include <arm/jit_interface.h>
#include <arm/jit_unicorn.h>

#include <dynarmic/A32/a32.h>

#include <memory>

namespace eka2l1 {
    class timing_system;
    class disasm;
    class memory_system;

    namespace hle {
        class lib_manager;
    }

    namespace arm {
        class arm_dynarmic_callback;

        class jit_dynarmic : public jit_interface {
            friend class arm_dynarmic_callback;
            
            jit_unicorn fallback_jit;

            std::unique_ptr<Dynarmic::A32::Jit> jit;
            std::unique_ptr<arm_dynarmic_callback> cb;

            timing_system *timing;
            disasm *asmdis;
            memory_system *mem;

            hle::lib_manager *lib_mngr;
        public:
            bool execute_instructions(int num_instructions) override;

            timing_system *get_timing_sys() {
                return timing;
            }

            disasm *get_disasm_sys() {
                return asmdis;
            }

            memory_system *get_memory_sys() {
                return mem;
            }

            hle::lib_manager *get_lib_manager() {
                return lib_mngr;
            }

            jit_dynarmic(timing_system *sys, memory_system *mem, disasm *asmdis, hle::lib_manager *mngr);
            ~jit_dynarmic();

            void run() override;
            void stop() override;

            void step() override;

            uint32_t get_reg(size_t idx) override;
            uint64_t get_sp() override;
            uint64_t get_pc() override;
            uint64_t get_vfp(size_t idx) override;

            void set_reg(size_t idx, uint32_t val) override;
            void set_pc(uint64_t val) override;
            void set_sp(uint32_t val) override;
            void set_lr(uint64_t val) override;
            void set_vfp(size_t idx, uint64_t val) override;

            uint32_t get_cpsr() override;

            void save_context(thread_context &ctx) override;
            void load_context(const thread_context &ctx) override;

            void set_entry_point(address ep) override;
            address get_entry_point() override;

            void set_stack_top(address addr) override;
            address get_stack_top() override;

            void prepare_rescheduling() override;

            bool is_thumb_mode() override;

            void imb_range(address start, uint32_t len);
        };
    }
}