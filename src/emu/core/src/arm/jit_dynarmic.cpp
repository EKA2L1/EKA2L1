#include <arm/jit_dynarmic.h>
#include <common/log.h>

#include <core_timing.h>

namespace eka2l1 {
    namespace arm {
        class arm_dynarmic_callback : public Dynarmic::A32::UserCallbacks {
            jit_dynarmic &parent;
            uint64_t interpreted;

            int routine_ticks = 32;

            bool log_read = true;
            bool log_write = true;
            bool log_code = true;

        public:
            explicit arm_dynarmic_callback(jit_dynarmic &parent)
                : parent(parent) {}

            void set_routine_ticks(int new_ticks) {
                routine_ticks = new_ticks;
            }

            uint8_t MemoryRead8(Dynarmic::A32::VAddr addr) override {
                uint8_t ret = *parent.get_memory_sys()->get_addr<uint8_t>(addr);

                if (log_read) {
                    LOG_TRACE("[Dynarmic] Read uint8_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }

                return ret;
            }

            uint16_t MemoryRead16(Dynarmic::A32::VAddr addr) override {
                uint16_t ret = *parent.get_memory_sys()->get_addr<uint16_t>(addr);

                if (log_read) {
                    LOG_TRACE("[Dynarmic] Read uint16_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }
            }

            uint32_t MemoryRead32(Dynarmic::A32::VAddr addr) override {
                uint32_t ret = *parent.get_memory_sys()->get_addr<uint32_t>(addr);

                if (log_read) {
                    LOG_TRACE("[Dynarmic] Read uint32_t at address: 0x{:x}, val = 0x{:x}", addr, ret);
                }

                return ret;
            }

            void MemoryWrite8(Dynarmic::A32::VAddr addr, uint8_t value) override {
                *parent.get_memory_sys()->get_addr<uint8_t>(addr) = value;

                if (log_write) {
                    LOG_TRACE("[Dynarmic] Write uint8_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }
            }

            void MemoryWrite16(Dynarmic::A32::VAddr addr, uint16_t value) override {
                *parent.get_memory_sys()->get_addr<uint16_t>(addr) = value;

                if (log_write) {
                    LOG_TRACE("[Dynarmic] Write uint16_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }
            }

            void MemoryWrite32(Dynarmic::A32::VAddr addr, uint32_t value) override {
                *parent.get_memory_sys()->get_addr<uint32_t>(addr) = value;

                if (log_write) {
                    LOG_TRACE("[Dynarmic] Write uint32_t at addr: 0x{:x}, val = 0x{:x}", addr, value);
                }
            }

            void InterpreterFallback(Dynarmic::A32::VAddr addr, size_t num_insts) {
                jit_interface::thread_context context;
                parent.save_context(context);
                parent.fallback_jit.load_context(context);
                parent.fallback_jit.execute_instructions(num_insts);
                parent.fallback_jit.save_context(context);
                parent.load_context(context);

                interpreted += num_insts;
            }

            void ExceptionRaised(uint32_t pc, Dynarmic::A32::Exception exception) override {
                switch (exception) {
                case Dynarmic::A32::Exception::Breakpoint:
                    return;
                default:
                    LOG_WARN("Exception Raised at pc = 0x{:x}", pc);
                }
            }

            void CallSVC(uint32_t svc) override {
                hle::lib_manager *mngr = parent.get_lib_manager();

                if (svc == 0) {
                    uint32_t val = *parent.get_memory_sys()->get_addr<uint32_t>(parent.get_pc() + 4);
                    bool res = mngr->call_hle(val);

                    if (!res) {
                        LOG_WARN("Unimplemented method!");
                    }

                    return;
                } else if (svc == 1) {
                    // Custom call
                    uint32_t val = *parent.get_memory_sys()->get_addr<uint32_t>(parent.get_pc() + 4);
                    bool res = mngr->call_custom_hle(val);

                    return;
                }

                bool res = mngr->call_svc(svc);

                if (!res) {
                    LOG_INFO("Unimplemented SVC call: 0x{:x}", svc);
                }
            }

            void AddTicks(uint64_t ticks) override {
                parent.get_timing_sys()->add_ticks(ticks - interpreted);
                routine_ticks -= ticks + interpreted;

                interpreted = 0;
            }

            uint64_t GetTicksRemaining() override {
                uint64_t res = static_cast<uint64_t>(std::max(routine_ticks, 0));

                if (res <= 0) {
                    routine_ticks = 32;
                }

                return res;
            }
        };

        std::unique_ptr<Dynarmic::A32::Jit> make_jit(std::unique_ptr<arm_dynarmic_callback> &callback) {
            Dynarmic::A32::UserConfig config;
            config.callbacks = callback.get();

            return std::make_unique<Dynarmic::A32::Jit>(config);
        }

        jit_dynarmic::jit_dynarmic(timing_system *sys, memory_system *mem, disasm *asmdis, hle::lib_manager *mngr)
            : timing(sys)
            , mem(mem)
            , asmdis(asmdis)
            , lib_mngr(mngr)
            , fallback_jit(sys, mem, asmdis, mngr)
            , cb(std::make_unique<arm_dynarmic_callback>(*this))
            , jit(make_jit(cb)) {
        }

        jit_dynarmic::~jit_dynarmic() {}

        bool jit_dynarmic::execute_instructions(int num_instructions) {
            cb->set_routine_ticks(num_instructions);
            jit->Run();
        }

        void jit_dynarmic::run() {
            jit->Run();
        }

        void jit_dynarmic::stop() {
        }

        void jit_dynarmic::step() {
            cb->InterpreterFallback(get_pc(), 1);
        }

        uint32_t jit_dynarmic::get_reg(size_t idx) {
            return jit->Regs[idx];
        }

        uint64_t jit_dynarmic::get_sp() {
            return jit->Regs[13];
        }

        uint64_t jit_dynarmic::get_pc() {
            return jit->Regs[15];
        }

        uint64_t jit_dynarmic::get_vfp(size_t idx) {
            return 0;
        }

        void jit_dynarmic::set_reg(size_t idx, uint32_t val) {
            jit->Regs[idx] = val;
        }

        void jit_dynarmic::set_pc(uint64_t val) {
            jit->Regs[15] = val;
        }

        void jit_dynarmic::set_sp(uint32_t val) {
            jit->Regs[13] = val;
        }

        void jit_dynarmic::set_lr(uint64_t val) {
            jit->Regs[14] = val;
        }

        void jit_dynarmic::set_vfp(size_t idx, uint64_t val) {
        }

        uint32_t jit_dynarmic::get_cpsr() {
            return jit->Cpsr();
        }

        void jit_dynarmic::save_context(thread_context &ctx) {
            ctx.cpsr = get_cpsr();
            ctx.cpu_registers = jit->Regs;
            ctx.pc = get_pc();
            ctx.sp = get_sp();
        }

        void jit_dynarmic::load_context(const thread_context &ctx) {
            jit->SetCpsr(ctx.cpsr);

            for (uint8_t i = 0; i < ctx.cpu_registers.size(); i++) {
                jit->Regs[i] = ctx.cpu_registers[i];
            }

            set_pc(ctx.pc);
            set_sp(ctx.sp);
        }

        void jit_dynarmic::set_entry_point(address ep) {
        }

        address jit_dynarmic::get_entry_point() {
        }

        void jit_dynarmic::set_stack_top(address addr) {
            set_sp(addr);
        }

        address jit_dynarmic::get_stack_top() {
            return get_sp();
        }

        void jit_dynarmic::prepare_rescheduling() {
        }

        bool jit_dynarmic::is_thumb_mode() {
            return get_cpsr() & 0x20;
        }
    }
}