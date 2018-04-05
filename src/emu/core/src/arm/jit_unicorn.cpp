#include <core/arm/jit_unicorn.h>
#include <cassert>

namespace eka2l1 {
    namespace arm {
        jit_unicorn::jit_unicorn() {
            uc_err err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &engine);
            assert(err == UC_ERR_OK);
        }

        jit_unicorn::~jit_unicorn() {
            uc_close(engine);
        }



        void jit_unicorn::run() {

        }

        void jit_unicorn::stop() {

        }

        void jit_unicorn::step() {

        }

        uint64_t jit_unicorn::get_reg(size_t idx) {
            return 0;
        }

        uint64_t jit_unicorn::get_sp() {
            return 0;
        }

        uint64_t jit_unicorn::get_pc() {
            return 0;
        }

        uint128_t jit_unicorn::get_vfp(size_t idx) {
            uint128_t temp;
            return temp;
        }

        void jit_unicorn::set_reg(size_t idx, uint64_t val) {

        }

        void jit_unicorn::set_pc(uint64_t val) {

        }

        void jit_unicorn::set_lr(uint64_t val) {

        }

        void jit_unicorn::set_vfp(size_t idx, uint128_t val) {

        }

        uint32_t jit_unicorn::get_cpsr() {
            return 0;
        }

        void jit_unicorn::save_context(thread_context& ctx) {

        }

        void jit_unicorn::load_context(const thread_context& ctx) {

        }
    }
}
