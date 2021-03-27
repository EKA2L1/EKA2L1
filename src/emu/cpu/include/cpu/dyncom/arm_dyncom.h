// Copyright 2014 Citra Emulator Project
// Copyright 2021 EKA2L1 Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <cpu/arm_interface.h>
#include <cpu/dyncom/armstate.h>
#include <cpu/12l1r/tlb.h>

namespace eka2l1::arm {
    class dyncom_core final : public core {
    private:
        arm::exclusive_monitor *monitor_;
        std::unique_ptr<ARMul_State> state_;
        r12l1::tlb mem_cache_;

        std::uint32_t ticks_executed_;

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
        uint32_t get_lr() override;
        void set_cpsr(uint32_t val) override;

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

        void set_asid(std::uint8_t num) override;
        std::uint8_t get_asid() const override;
        std::uint8_t get_max_asid_available() const override;
    };
}