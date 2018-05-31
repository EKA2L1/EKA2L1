#pragma once

#include <arm/jit_interface.h>
#include <unicorn/unicorn.h>

namespace eka2l1 {
    class timing_system;
    class disasm;
    class memory_system;

    namespace hle {
        class lib_manager;
    }

    namespace arm {
        class jit_unicorn : public jit_interface {
            uc_engine *engine;
            address epa;

            timing_system *timing;
            disasm *asmdis;
            memory_system *mem;

            hle::lib_manager *lib_mngr;

        private:
            bool execute_instructions(int num_instructions);

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

            jit_unicorn(timing_system *sys, memory_system *mem, disasm *asmdis, hle::lib_manager *mngr);
            ~jit_unicorn();

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
        };
    }
}
