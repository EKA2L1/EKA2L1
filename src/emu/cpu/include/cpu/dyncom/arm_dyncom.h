// Copyright 2014 Citra Emulator Project
// Copyright 2021 EKA2L1 Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <cpu/12l1r/tlb.h>
#include <cpu/arm_interface.h>
#include <cpu/dyncom/armstate.h>

namespace eka2l1::arm {
#if defined(EKA2L1_DYNCOM_DIFFTEST)
    // Test-only instrumentation for the translation-time loop accelerator. It
    // only ever attaches during block translation, so a harness must be able to
    // assert it was actually exercised rather than silently skipped.
    void dyncom_reset_loop_accel_counters_for_test();
    std::uint64_t dyncom_loop_accel_attaches_for_test();
    std::uint64_t dyncom_loop_accel_bulk_iterations_for_test();
#endif

    class dyncom_core final : public core {
    private:
        arm::exclusive_monitor *monitor_;
        std::unique_ptr<ARMul_State> state_;
        r12l1::tlb mem_cache_;

        std::uint32_t ticks_executed_;

        // True once the scheduler starts feeding us asids (primary core). While
        // false (e.g. the dyncom interpreter embedded in another backend as a
        // fallback) we keep the old behaviour of wiping the translation cache on
        // every load_context, since nobody tells us when the address space flips.
        bool asid_instruction_cache_ = false;

    public:
        explicit dyncom_core(arm::exclusive_monitor *monitor, const std::size_t page_bits);
        ~dyncom_core() override;

        arm::exclusive_monitor *exmonitor() {
            return monitor_;
        }

        r12l1::tlb *mem_cache() {
            return &mem_cache_;
        }

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

        void set_asid(const std::uint32_t asid) override;

        std::uint32_t get_num_instruction_executed() override;

        bool should_clear_old_memory_map() const override {
            return false;
        }
    };
}