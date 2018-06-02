#include <hle/return_val.h>

namespace eka2l1 {
    namespace hle {
        template <>
        void write_return_value(arm::jitter &cpu, uint16_t ret) {
            cpu->set_reg(0, ret);
        }

        template <>
        void write_return_value(arm::jitter &cpu, int32_t ret) {
            cpu->set_reg(0, ret);
        }

        template <>
        void write_return_value(arm::jitter &cpu, uint32_t ret) {
            cpu->set_reg(0, ret);
        }
    }
}