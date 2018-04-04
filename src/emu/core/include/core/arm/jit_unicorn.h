#pragma once

#include <unicorn/unicorn.h>
#include <core/arm/jit_interface.h>

namespace eka2l1 {
    namespace arm {
        class jit_unicorn: public jit_interface {
            uc_engine* engine;
        public:
            jit_unicorn();
            ~jit_unicorn();

            void run() override;
            void stop() override;

            void step() override;

            uint64_t get_reg(size_t idx) override;
            uint64_t get_sp() override;
            uint64_t get_pc() override;
            uint128_t get_vfp(size_t idx) override;

            void set_reg(size_t idx, uint64_t val) override;
            void set_pc(uint64_t val) override;
            void set_lr(uint64_t val) override;
            void set_vfp(size_t idx, uint128_t val) override;

            uint32_t get_cpsr() override;

            void save_context(thread_context& ctx) override;
            void load_context(const thread_context& ctx) override;
        };
    }
}
