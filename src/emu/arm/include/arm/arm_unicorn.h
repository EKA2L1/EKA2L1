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

#include <arm/arm_interface.h>
#include <gdbstub/gdbstub.h>

#include <unicorn/unicorn.h>

namespace eka2l1 {
    class timing_system;
    class manager_system;
    class kernel_system;
    class memory_system;

    class disasm;
    class gdbstub;

    namespace hle {
        class lib_manager;
    }

    namespace manager {
        struct config_state;
    }

    namespace arm {
        class arm_unicorn : public arm_interface {
            uc_engine *engine;
            address epa;

            timing_system *timing;
            disasm *asmdis;
            memory_system *mem;
            kernel_system *kern;

            hle::lib_manager *lib_mngr;
            gdbstub *stub;

            manager_system *mngr;

            breakpoint_address last_breakpoint;
            bool last_breakpoint_hit { false };
            address breakpoint_hit_addr { 0 };

        public:
            manager::config_state *conf;
            std::uint32_t num_insts_runned { 0 };
            
            bool execute_instructions(uint32_t num_instructions);

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

            gdbstub *get_gdb_stub() {
                return stub;
            }

            manager_system *get_manager_sys() {
                return mngr;
            }

            std::uint32_t get_num_instruction_executed() override {
                return num_insts_runned;
            }

            bool did_last_breakpoint_hit() const {
                return last_breakpoint_hit;
            }
            
            void record_break(breakpoint_address bkpt) {
                last_breakpoint = bkpt;
                last_breakpoint_hit = true;
            }

            void set_breakpoint_hit_address(const address addr) {
                breakpoint_hit_addr = addr;
            }

            arm_unicorn(kernel_system *kern, timing_system *sys, manager::config_state *conf, 
                manager_system *mngr, memory_system *mem, disasm *asmdis, hle::lib_manager *lmngr, gdbstub *stub);

            ~arm_unicorn();

            void run() override;
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
        };
    }
}
