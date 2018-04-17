#include <core_arm.h>
#include <arm/jit_unicorn.h>

namespace eka2l1 {
    namespace core_arm {
        std::shared_ptr<arm::jit_interface> arm_cpu;

        std::shared_ptr<arm::jit_interface> jit_factory(core_arm_type arm_type) {
            switch (arm_type) {
                case core_arm_type::unicorn:
                    return std::shared_ptr<arm::jit_unicorn>();

                default:
                    return nullptr;
            }
        }

        void init(core_arm_type arm_type) {
            arm_cpu = std::move(jit_factory(arm_type));
        }

        void run() {
            arm_cpu->run();
        }

        void stop() {
            arm_cpu->stop();
        }

        void shutdown() {
            arm_cpu.reset();
        }

        void step() {
            arm_cpu->step();
        }

        uint32_t get_reg(size_t idx) {
            return arm_cpu->get_reg(idx);
        }

        uint64_t get_sp() {
            return arm_cpu->get_sp();
        }

        uint64_t get_pc() {
            return arm_cpu->get_pc();
        }

        uint64_t get_vfp(size_t idx) {
            return arm_cpu->get_vfp(idx);
        }

        void set_reg(size_t idx, uint32_t val) {
            arm_cpu->set_reg(idx, val);
        }

        void set_pc(uint64_t val) {
            arm_cpu->set_pc(val);
        }

        void set_lr(uint64_t val) {
            arm_cpu->set_lr(val);
        }

        void set_vfp(size_t idx, uint64_t val) {
            arm_cpu->set_vfp(idx, val);
        }

        uint32_t get_cpsr() {
            return arm_cpu->get_cpsr();
        }

        void save_context(arm::jit_interface::thread_context& ctx) {
            arm_cpu->save_context(ctx);
        }

        void load_context(const arm::jit_interface::thread_context& ctx) {
            arm_cpu->load_context(ctx);
        }

        std::shared_ptr<arm::jit_interface> get_instance() {
            return arm_cpu;
        }
    }
}
