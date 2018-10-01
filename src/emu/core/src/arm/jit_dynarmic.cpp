#include <common/algorithm.h>
#include <common/log.h>

#include <core/arm/jit_dynarmic.h>

#include <core/core_mem.h>
#include <core/core_timing.h>
#include <core/hle/libmanager.h>

#include <dynarmic/A32/context.h>

namespace eka2l1 {
    namespace arm {
        class arm_dynarmic_callback : public Dynarmic::A32::UserCallbacks {
            jit_dynarmic &parent;
            uint64_t interpreted = 0;

            bool log_read = false;
            bool log_write = false;
            bool log_code = true;

        public:
            explicit arm_dynarmic_callback(jit_dynarmic &parent)
                : parent(parent) {}

            uint8_t MemoryRead8(Dynarmic::A32::VAddr addr) override {
                uint8_t ret = 0;
                parent.get_memory_sys()->read(addr, &ret, sizeof(uint8_t));

                if (log_read) {
                    LOG_TRACE("[Dynarmic] Read uint8_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }

                return ret;
            }

            uint16_t MemoryRead16(Dynarmic::A32::VAddr addr) override {
                uint16_t ret = 0;
                parent.get_memory_sys()->read(addr, &ret, sizeof(uint16_t));

                if (log_read) {
                    LOG_TRACE("[Dynarmic] Read uint16_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }

                return ret;
            }

            uint32_t MemoryRead32(Dynarmic::A32::VAddr addr) override {
                uint32_t ret = 0;
                parent.get_memory_sys()->read(addr, &ret, sizeof(uint32_t));

                if (log_read) {
                    LOG_TRACE("[Dynarmic] Read uint32_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }

                return ret;
            }

            uint64_t MemoryRead64(Dynarmic::A32::VAddr addr) override {
                uint64_t ret = 0;
                parent.get_memory_sys()->read(addr, &ret, sizeof(uint64_t));

                if (log_read) {
                    LOG_TRACE("[Dynarmic] Read uint64_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }

                return ret;
            }

            void MemoryWrite8(Dynarmic::A32::VAddr addr, uint8_t value) override {
                parent.get_memory_sys()->write(addr, &value, sizeof(value));

                if (log_write) {
                    LOG_TRACE("[Dynarmic] Write uint8_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }
            }

            void MemoryWrite16(Dynarmic::A32::VAddr addr, uint16_t value) override {
                parent.get_memory_sys()->write(addr, &value, sizeof(value));

                if (log_write) {
                    LOG_TRACE("[Dynarmic] Write uint16_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }
            }

            void MemoryWrite32(Dynarmic::A32::VAddr addr, uint32_t value) override {
                parent.get_memory_sys()->write(addr, &value, sizeof(value));

                if (log_write) {
                    LOG_TRACE("[Dynarmic] Write uint32_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }
            }

            void MemoryWrite64(Dynarmic::A32::VAddr addr, uint64_t value) override {
                parent.get_memory_sys()->write(addr, &value, sizeof(value));

                if (log_write) {
                    LOG_TRACE("[Dynarmic] Write uint64_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }
            }

            void InterpreterFallback(Dynarmic::A32::VAddr addr, size_t num_insts) override {
                jit_interface::thread_context context;
                parent.save_context(context);
                parent.fallback_jit.load_context(context);
                parent.fallback_jit.execute_instructions(static_cast<uint32_t>(num_insts));
                parent.fallback_jit.save_context(context);
                parent.load_context(context);

                interpreted += num_insts;
            }

            void ExceptionRaised(uint32_t pc, Dynarmic::A32::Exception exception) override {
                switch (exception) {
                case Dynarmic::A32::Exception::Breakpoint:
                case Dynarmic::A32::Exception::UndefinedInstruction:
                    return;
                default:
                    LOG_WARN("Exception Raised at pc = 0x{:x}", pc);
                    break;
                }
            }

            void CallSVC(uint32_t svc) override {
                hle::lib_manager *mngr = parent.get_lib_manager();

                if (svc == 0x900000) {
                    uint32_t val = *reinterpret_cast<uint32_t *>(parent.get_memory_sys()->get_real_pointer(parent.get_pc() + 4));
                    bool res = mngr->call_hle(val);

                    if (!res) {
                        LOG_WARN("Unimplemented method!");
                    }

                    return;
                } else if (svc == 0x900001) {
                    // Custom call
                    uint32_t val = *reinterpret_cast<uint32_t *>(parent.get_memory_sys()->get_real_pointer(parent.get_pc() + 4));
                    bool res = mngr->call_custom_hle(val);

                    return;
                } else if (svc == 0x900002) {
                    uint32_t call_addr = parent.get_pc();

                    if (call_addr && parent.is_thumb_mode()) {
                        call_addr -= 1;
                    } else {
                        call_addr -= 4;
                    }

                    bool res = parent.get_lib_manager()->call_hle(*parent.get_lib_manager()->get_sid(call_addr));

                    if (res) {
                        uint32_t lr = parent.get_reg(14);
                        parent.set_pc(lr);
                    }

                    return;
                }

                bool res = mngr->call_svc(svc);

                if (!res) {
                    LOG_INFO("Unimplemented SVC call: 0x{:x}", svc);
                }
            }

            void AddTicks(uint64_t ticks) override {
                parent.get_timing_sys()->add_ticks(ticks - interpreted);

                interpreted = 0;
            }

            uint64_t GetTicksRemaining() override {
                return eka2l1::common::max(parent.get_timing_sys()->get_downcount(), 0);
            }
        };

        std::unique_ptr<Dynarmic::A32::Jit> make_jit(std::array<uint8_t *, Dynarmic::A32::UserConfig::NUM_PAGE_TABLE_ENTRIES> &pages,
            std::unique_ptr<arm_dynarmic_callback> &callback, page_table *table) {
            Dynarmic::A32::UserConfig config;
            config.callbacks = callback.get();

            if (table) {
                using config_table_array = decltype(*config.page_table);

                for (size_t i = 0; i < table->pointers.size(); i++) {
                    pages[i] = table->pointers[i].get();
                }

                config.page_table = &pages;
            }

            return std::make_unique<Dynarmic::A32::Jit>(config);
        }

        jit_dynarmic::jit_dynarmic(timing_system *sys, manager_system *mngr, memory_system *mem, disasm *asmdis, hle::lib_manager *lmngr)
            : timing(sys)
            , mem(mem)
            , asmdis(asmdis)
            , lib_mngr(lmngr)
            , mngr(mngr)
            , fallback_jit(sys, mngr, mem, asmdis, lmngr)
            , cb(std::make_unique<arm_dynarmic_callback>(*this)) {
            jit = std::move(make_jit(page_table_dyn, cb, mem->get_current_page_table()));
        }

        jit_dynarmic::~jit_dynarmic() {}

        void jit_dynarmic::run() {
            jit->Run();
        }

        void jit_dynarmic::stop() {
            jit->HaltExecution();
        }

        void jit_dynarmic::step() {
            cb->InterpreterFallback(get_pc(), 1);
        }

        uint32_t jit_dynarmic::get_reg(size_t idx) {
            return jit->Regs()[idx];
        }

        uint32_t jit_dynarmic::get_sp() {
            return jit->Regs()[13];
        }

        uint32_t jit_dynarmic::get_pc() {
            return jit->Regs()[15];
        }

        uint32_t jit_dynarmic::get_vfp(size_t idx) {
            return 0;
        }

        void jit_dynarmic::set_reg(size_t idx, uint32_t val) {
            jit->Regs()[idx] = val;
        }

        void jit_dynarmic::set_pc(uint32_t val) {
            jit->Regs()[15] = val;
        }

        void jit_dynarmic::set_sp(uint32_t val) {
            jit->Regs()[13] = val;
        }

        void jit_dynarmic::set_lr(uint32_t val) {
            jit->Regs()[14] = val;
        }

        void jit_dynarmic::set_vfp(size_t idx, uint32_t val) {
        }

        uint32_t jit_dynarmic::get_lr() {
            return jit->Regs()[14];
        }

        uint32_t jit_dynarmic::get_cpsr() {
            return jit->Cpsr();
        }

        void jit_dynarmic::set_cpsr(uint32_t val) {
            jit->SetCpsr(val);
        }

        void jit_dynarmic::save_context(thread_context &ctx) {
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
        }

        void jit_dynarmic::load_context(const thread_context &ctx) {
            for (uint8_t i = 0; i < 16; i++) {
                jit->Regs()[i] = ctx.cpu_registers[i];
            }

            set_sp(ctx.sp);
            set_pc(ctx.pc);
            set_lr(ctx.lr);
            set_cpsr(ctx.cpsr);
        }

        void jit_dynarmic::set_entry_point(address ep) {
        }

        address jit_dynarmic::get_entry_point() {
            return 0;
        }

        void jit_dynarmic::set_stack_top(address addr) {
            set_sp(addr);
        }

        address jit_dynarmic::get_stack_top() {
            return get_sp();
        }

        void jit_dynarmic::prepare_rescheduling() {
            if (jit->IsExecuting()) {
                jit->HaltExecution();
            }
        }

        bool jit_dynarmic::is_thumb_mode() {
            return get_cpsr() & 0x20;
        }

        void jit_dynarmic::imb_range(address start, uint32_t len) {
            jit->InvalidateCacheRange(start, len);
        }

        void jit_dynarmic::page_table_changed() {
            arm::jit_interface::thread_context ctx;
            save_context(ctx);

            jit = std::move(make_jit(page_table_dyn, cb, mem->get_current_page_table()));
            load_context(ctx);
        }

        void jit_dynarmic::map_backing_mem(address vaddr, size_t size, uint8_t *ptr, prot protection) {
            for (std::size_t i = 0; i < size / mem->get_page_size(); i++) {
                page_table_dyn[vaddr / mem->get_page_size()] = ptr + mem->get_page_size() * i;
            }

            fallback_jit.map_backing_mem(vaddr, size, ptr, protection);
        }

        void jit_dynarmic::unmap_memory(address addr, size_t size) {
            for (std::size_t i = 0; i < size / mem->get_page_size(); i++) {
                page_table_dyn[addr / mem->get_page_size()] = nullptr;
            }

            fallback_jit.unmap_memory(addr, size);
        }
    }
}