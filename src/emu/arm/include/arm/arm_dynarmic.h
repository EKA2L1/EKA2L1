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

#include <arm/arm_interface.h>
#include <arm/arm_unicorn.h>

#include <dynarmic/A32/a32.h>
#include <dynarmic/A32/config.h>

#include <map>
#include <memory>

namespace eka2l1 {
    class kernel_system;
    class timing_system;
    class manager_system;
    class memory_system;

    class disasm;
    class gdbstub;

    namespace hle {
        class lib_manager;
    }

    namespace arm {
        class arm_dynarmic_callback;

        /* UNUSABLE */
        class arm_dynarmic : public arm_interface {
            friend class arm_dynarmic_callback;

            arm_unicorn fallback_jit;

            std::unique_ptr<Dynarmic::A32::Jit> jit;
            std::unique_ptr<arm_dynarmic_callback> cb;

            disasm *asmdis;
            timing_system *timing;
            manager_system *mngr;
            memory_system *mem;
            kernel_system *kern;

            hle::lib_manager *lib_mngr;

            gdbstub *stub;
            debugger_ptr debugger;

			std::array<std::uint8_t*, Dynarmic::A32::UserConfig::NUM_PAGE_TABLE_ENTRIES>
				page_dyn;

        public:
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

            arm_dynarmic(kernel_system *kern, timing_system *sys, manager_system *mngr, memory_system *mem, 
                disasm *asmdis, hle::lib_manager *lmngr, gdbstub *stub, debugger_ptr debugger);
                
            ~arm_dynarmic();

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

            void imb_range(address start, uint32_t len);

            void page_table_changed() override;

            void map_backing_mem(address vaddr, size_t size, uint8_t *ptr, prot protection) override;

            void unmap_memory(address addr, size_t size) override;

            void clear_instruction_cache() override;

            void imb_range(address addr, std::size_t size) override;

            bool should_clear_old_memory_map() const override {
                return false;
            }
        };
    }
}