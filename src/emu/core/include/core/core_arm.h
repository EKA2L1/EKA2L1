#pragma once

#include <arm/jit_interface.h>
#include <memory>

namespace eka2l1 {
    namespace core_arm {
        enum core_arm_type {
            dynarmic = 0,
            unicorn = 1
        };

        void init(core_arm_type arm_type);
        void run();
        void stop();
        void shutdown();

        void step();

        uint32_t get_reg(size_t idx);
        uint64_t get_sp();
        uint64_t get_pc();
        uint64_t get_vfp(size_t idx);

        void set_reg(size_t idx, uint32_t val);
        void set_pc(uint64_t val);
        void set_lr(uint64_t val);
        void set_vfp(size_t idx, uint64_t val);

        uint32_t get_cpsr();

        void save_context(arm::jit_interface::thread_context &ctx);
        void load_context(const arm::jit_interface::thread_context &ctx);

        std::shared_ptr<arm::jit_interface> get_instance();
    }
}
