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

#include <common/algorithm.h>
#include <common/configure.h>
#include <common/log.h>

#include <cpu/arm_dynarmic.h>
#include <cpu/arm_utils.h>

#include <dynarmic/A32/context.h>
#include <dynarmic/A32/coprocessor.h>

namespace eka2l1::arm {
    class dynarmic_core_cp15 : public Dynarmic::A32::Coprocessor {
        std::uint32_t wrwr;

    public:
        using coproc_reg = Dynarmic::A32::CoprocReg;

        explicit dynarmic_core_cp15()
            : wrwr(0) {
        }

        ~dynarmic_core_cp15() override {
        }

        void set_wrwr(const std::uint32_t wrwr_val) {
            wrwr = wrwr_val;
        }

        const std::uint32_t get_wrwr() const {
            return wrwr;
        }

        std::optional<Callback> CompileInternalOperation(bool two, unsigned opc1, coproc_reg CRd,
            coproc_reg CRn, coproc_reg CRm,
            unsigned opc2) override {
            return std::nullopt;
        }

        CallbackOrAccessOneWord CompileSendOneWord(bool two, unsigned opc1, coproc_reg CRn,
            coproc_reg CRm, unsigned opc2) override {
            return CallbackOrAccessOneWord{};
        }

        CallbackOrAccessTwoWords CompileSendTwoWords(bool two, unsigned opc, coproc_reg CRm) override {
            return CallbackOrAccessTwoWords{};
        }

        CallbackOrAccessOneWord CompileGetOneWord(bool two, unsigned opc1, coproc_reg CRn, coproc_reg CRm,
            unsigned opc2) override {
            if ((CRn == coproc_reg::C13) && (CRm == coproc_reg::C0) && (opc1 == 0) && (opc2 == 2)) {
                return &wrwr;
            }

            return CallbackOrAccessOneWord{};
        }

        CallbackOrAccessTwoWords CompileGetTwoWords(bool two, unsigned opc, coproc_reg CRm) override {
            return CallbackOrAccessTwoWords{};
        }

        std::optional<Callback> CompileLoadWords(bool two, bool long_transfer, coproc_reg CRd,
            std::optional<std::uint8_t> option) override {
            return std::nullopt;
        }

        std::optional<Callback> CompileStoreWords(bool two, bool long_transfer, coproc_reg CRd,
            std::optional<std::uint8_t> option) override {
            return std::nullopt;
        }
    };

    class dynarmic_core_callback : public Dynarmic::A32::UserCallbacks {
        std::shared_ptr<dynarmic_core_cp15> cp15;

        dynarmic_core &parent;
        std::uint64_t interpreted;

    public:
        explicit dynarmic_core_callback(dynarmic_core &parent, std::shared_ptr<dynarmic_core_cp15> cp15)
            : cp15(cp15)
            , parent(parent)
            , interpreted(0) {}

        dynarmic_core_cp15 *get_cp15() {
            return cp15.get();
        }

        void invalid_memory_read(const Dynarmic::A32::VAddr addr) {
            parent.exception_handler(exception_type_access_violation_read, addr);
        }

        void invalid_memory_write(const Dynarmic::A32::VAddr addr) {
            parent.exception_handler(exception_type_access_violation_write, addr);
        }

        void handle_read_status(const bool status, const Dynarmic::A32::VAddr addr) {
            if (!status) {
                invalid_memory_read(addr);
            }
        }

        void handle_write_status(const bool status, const Dynarmic::A32::VAddr addr) {
            if (!status) {
                invalid_memory_write(addr);
            }
        }

        std::uint32_t MemoryReadCode(Dynarmic::A32::VAddr addr) override {
            std::uint32_t code_result = 0;
            constexpr std::uint32_t UNDEFINED_WORD = 0xE11EFF2F;

            bool status = parent.read_32bit(addr, &code_result);
            handle_read_status(status, addr);

            return status ? code_result : UNDEFINED_WORD;
        }

        uint8_t MemoryRead8(Dynarmic::A32::VAddr addr) override {
            std::uint8_t ret = 0;
            handle_read_status(parent.read_8bit(addr, &ret), addr);

            return ret;
        }

        uint16_t MemoryRead16(Dynarmic::A32::VAddr addr) override {
            std::uint16_t ret = 0;
            handle_read_status(parent.read_16bit(addr, &ret), addr);

            return ret;
        }

        uint32_t MemoryRead32(Dynarmic::A32::VAddr addr) override {
            std::uint32_t ret = 0;
            handle_read_status(parent.read_32bit(addr, &ret), addr);

            return ret;
        }

        uint64_t MemoryRead64(Dynarmic::A32::VAddr addr) override {
            std::uint64_t ret = 0;
            handle_read_status(parent.read_64bit(addr, &ret), addr);

            return ret;
        }

        void MemoryWrite8(Dynarmic::A32::VAddr addr, uint8_t value) override {
            handle_write_status(parent.write_8bit(addr, &value), addr);
        }

        void MemoryWrite16(Dynarmic::A32::VAddr addr, uint16_t value) override {
            handle_write_status(parent.write_16bit(addr, &value), addr);
        }

        void MemoryWrite32(Dynarmic::A32::VAddr addr, uint32_t value) override {
            handle_write_status(parent.write_32bit(addr, &value), addr);
        }

        void MemoryWrite64(Dynarmic::A32::VAddr addr, uint64_t value) override {
            handle_write_status(parent.write_64bit(addr, &value), addr);
        }

        bool MemoryWriteExclusive8(Dynarmic::A32::VAddr addr, std::uint8_t value, std::uint8_t expected) {
            return parent.exclusive_write_8bit(addr, value, expected);
        }

        bool MemoryWriteExclusive16(Dynarmic::A32::VAddr addr, std::uint16_t value, std::uint16_t expected) {
            return parent.exclusive_write_16bit(addr, value, expected);
        }

        bool MemoryWriteExclusive32(Dynarmic::A32::VAddr addr, std::uint32_t value, std::uint32_t expected) {
            return parent.exclusive_write_32bit(addr, value, expected);
        }

        bool MemoryWriteExclusive64(Dynarmic::A32::VAddr addr, std::uint64_t value, std::uint64_t expected) {
            return parent.exclusive_write_64bit(addr, value, expected);
        }

        void InterpreterFallback(Dynarmic::A32::VAddr addr, size_t num_insts) override {
            LOG_ERROR("Interpreter fallback!");
        }

        void ExceptionRaised(uint32_t pc, Dynarmic::A32::Exception exception) override {
            switch (exception) {
            case Dynarmic::A32::Exception::UndefinedInstruction: {
                parent.exception_handler(exception_type_undefined_inst, pc);
                break;
            }

            case Dynarmic::A32::Exception::Breakpoint: {
                parent.exception_handler(exception_type_breakpoint, pc);
                return;
            }

            case Dynarmic::A32::Exception::UnpredictableInstruction:
                parent.exception_handler(exception_type_unpredictable, pc);
                return;

            default: {
                parent.exception_handler(exception_type_unk, pc);
                break;
            }
            }
        }

        void CallSVC(std::uint32_t svc) override {
            parent.system_call_handler(svc);
        }

        void AddTicks(uint64_t ticks) override {
            parent.ticks_executed += static_cast<std::uint32_t>(ticks - interpreted);
            interpreted = 0;
        }

        std::uint64_t GetTicksRemaining() override {
            return static_cast<std::uint64_t>(common::max<std::int64_t>(static_cast<std::int64_t>(parent.ticks_target)
                    - parent.ticks_executed, 0));
        }
    };

    std::unique_ptr<Dynarmic::A32::Jit> make_jit(std::unique_ptr<dynarmic_core_callback> &callback, void *table,
        std::shared_ptr<dynarmic_core_cp15> cp15, Dynarmic::ExclusiveMonitor *monitor) {
        Dynarmic::A32::UserConfig config;
        config.callbacks = callback.get();
        config.coprocessors[15] = cp15;
        config.page_table = reinterpret_cast<decltype(config.page_table)>(table);
        config.global_monitor = monitor;

        return std::make_unique<Dynarmic::A32::Jit>(config);
    }

    dynarmic_core::dynarmic_core(arm::exclusive_monitor *monitor) {
        std::shared_ptr<dynarmic_core_cp15> cp15 = std::make_shared<dynarmic_core_cp15>();
        cb = std::make_unique<dynarmic_core_callback>(*this, cp15);

        std::fill(page_dyn.begin(), page_dyn.end(), nullptr);

        auto monitor_bb = reinterpret_cast<dynarmic_exclusive_monitor*>(monitor);
        jit = make_jit(cb, &page_dyn, cp15, &monitor_bb->monitor_);
    }

    dynarmic_core::~dynarmic_core() {
    }

    void dynarmic_core::run(const std::uint32_t instruction_count) {
        ticks_executed = 0;
        ticks_target = instruction_count;

        jit->Run();
    }

    void dynarmic_core::stop() {
        jit->HaltExecution();
    }

    void dynarmic_core::step() {
        jit->Step();
    }

    uint32_t dynarmic_core::get_reg(size_t idx) {
        return jit->Regs()[idx];
    }

    uint32_t dynarmic_core::get_sp() {
        return jit->Regs()[13];
    }

    uint32_t dynarmic_core::get_pc() {
        return jit->Regs()[15];
    }

    uint32_t dynarmic_core::get_vfp(size_t idx) {
        return 0;
    }

    void dynarmic_core::set_reg(size_t idx, uint32_t val) {
        jit->Regs()[idx] = val;
    }

    void dynarmic_core::set_pc(uint32_t val) {
        jit->Regs()[15] = val;
    }

    void dynarmic_core::set_sp(uint32_t val) {
        jit->Regs()[13] = val;
    }

    void dynarmic_core::set_lr(uint32_t val) {
        jit->Regs()[14] = val;
    }

    void dynarmic_core::set_vfp(size_t idx, uint32_t val) {
    }

    uint32_t dynarmic_core::get_lr() {
        return jit->Regs()[14];
    }

    uint32_t dynarmic_core::get_cpsr() {
        return jit->Cpsr();
    }

    void dynarmic_core::set_cpsr(uint32_t val) {
        jit->SetCpsr(val);
    }

    void dynarmic_core::save_context(thread_context &ctx) {
        ctx.cpsr = get_cpsr();

        for (uint8_t i = 0; i < 16; i++) {
            ctx.cpu_registers[i] = get_reg(i);
        }

        ctx.pc = get_pc();
        ctx.sp = get_sp();
        ctx.lr = get_lr();

        if (!ctx.pc) {
            LOG_WARN("Dynarmic save context with PC = 0");
        }

        ctx.wrwr = cb->get_cp15()->get_wrwr();
    }

    void dynarmic_core::load_context(const thread_context &ctx) {
        for (uint8_t i = 0; i < 16; i++) {
            jit->Regs()[i] = ctx.cpu_registers[i];
        }

        set_sp(ctx.sp);
        set_pc(ctx.pc);
        set_lr(ctx.lr);
        set_cpsr(ctx.cpsr);

        cb->get_cp15()->set_wrwr(ctx.wrwr);
    }

    void dynarmic_core::set_entry_point(address ep) {
    }

    address dynarmic_core::get_entry_point() {
        return 0;
    }

    void dynarmic_core::set_stack_top(address addr) {
        set_sp(addr);
    }

    address dynarmic_core::get_stack_top() {
        return get_sp();
    }

    void dynarmic_core::prepare_rescheduling() {
        if (jit->IsExecuting()) {
            jit->HaltExecution();
        }
    }

    bool dynarmic_core::is_thumb_mode() {
        return get_cpsr() & 0x20;
    }

    void dynarmic_core::page_table_changed() {
    }

    void dynarmic_core::map_backing_mem(address vaddr, size_t size, uint8_t *ptr, prot protection) {
        const std::uint32_t psize = 0x1000;
        const std::uint32_t pstart = vaddr / psize;

        for (std::size_t i = 0; i < size / psize; i++) {
            page_dyn[pstart + i] = ptr + i * psize;
        }
    }

    void dynarmic_core::unmap_memory(address addr, size_t size) {
        const std::uint32_t psize = 0x1000;
        const std::uint32_t pstart = addr / psize;

        for (std::size_t i = 0; i < size / psize; i++) {
            page_dyn[pstart + i] = nullptr;
        }
    }

    void dynarmic_core::clear_instruction_cache() {
        jit->ClearCache();
    }

    void dynarmic_core::imb_range(address addr, std::size_t size) {
        jit->InvalidateCacheRange(addr, size);
    }

    std::uint32_t dynarmic_core::get_num_instruction_executed() {
        return ticks_executed;
    }

    void dynarmic_core::set_asid(std::uint8_t num) {
        return jit->SetAsid(num);
    }

    std::uint8_t dynarmic_core::get_asid() const {
        return jit->Asid();
    }

    std::uint8_t dynarmic_core::get_max_asid_available() const {
        return jit->MaxAsidAvailable();
    }

    dynarmic_exclusive_monitor::dynarmic_exclusive_monitor(const std::size_t processor_count)
        : monitor_(processor_count) {
    }

    std::uint8_t dynarmic_exclusive_monitor::exclusive_read8(core *cc, address vaddr) {
        return monitor_.ReadAndMark<std::uint8_t>(cc->core_number(), vaddr, [&]() -> std::uint8_t {
            // TODO: Access violation if there is
            std::uint8_t val = 0;
            read_8bit(cc, vaddr, &val);

            return val;
        });
    }
    
    std::uint16_t dynarmic_exclusive_monitor::exclusive_read16(core *cc, address vaddr) {
        return monitor_.ReadAndMark<std::uint16_t>(cc->core_number(), vaddr, [&]() -> std::uint16_t {
            // TODO: Access violation if there is
            std::uint16_t val = 0;
            read_16bit(cc, vaddr, &val);

            return val;
        });
    }

    std::uint32_t dynarmic_exclusive_monitor::exclusive_read32(core *cc, address vaddr) {
        return monitor_.ReadAndMark<std::uint32_t>(cc->core_number(), vaddr, [&]() -> std::uint32_t {
            // TODO: Access violation if there is
            std::uint32_t val = 0;
            read_32bit(cc, vaddr, &val);

            return val;
        });
    }

    std::uint64_t dynarmic_exclusive_monitor::exclusive_read64(core *cc, address vaddr) {
        return monitor_.ReadAndMark<std::uint64_t>(cc->core_number(), vaddr, [&]() -> std::uint64_t {
            // TODO: Access violation if there is
            std::uint64_t val = 0;
            read_64bit(cc, vaddr, &val);

            return val;
        });
    }
    
    void dynarmic_exclusive_monitor::clear_exclusive() {
        monitor_.Clear();
    }

    bool dynarmic_exclusive_monitor::exclusive_write8(core *cc, address vaddr, std::uint8_t value) {
        return monitor_.DoExclusiveOperation<std::uint8_t>(cc->core_number(), vaddr, [&](std::uint8_t expected) -> bool {
            const std::int32_t res = write_8bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }

    bool dynarmic_exclusive_monitor::exclusive_write16(core *cc, address vaddr, std::uint16_t value) {
        return monitor_.DoExclusiveOperation<std::uint16_t>(cc->core_number(), vaddr, [&](std::uint16_t expected) -> bool {
            const std::int32_t res = write_16bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }

    bool dynarmic_exclusive_monitor::exclusive_write32(core *cc, address vaddr, std::uint32_t value) {
        return monitor_.DoExclusiveOperation<std::uint32_t>(cc->core_number(), vaddr, [&](std::uint32_t expected) -> bool {
            const std::int32_t res = write_32bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }
    
    bool dynarmic_exclusive_monitor::exclusive_write64(core *cc, address vaddr, std::uint64_t value) {
        return monitor_.DoExclusiveOperation<std::uint64_t>(cc->core_number(), vaddr, [&](std::uint64_t expected) -> bool {
            const std::int32_t res = write_64bit(cc, vaddr, value, expected);

            // TODO: Parse access violation errors
            return res > 0;
        });
    }
}
