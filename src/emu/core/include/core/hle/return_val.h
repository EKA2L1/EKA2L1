#pragma once

#include <arm/jit_factory.h>
#include <ptr.h>

namespace eka2l1 {
    namespace hle {
        template <typename ret>
        void write_return_value(arm::jitter &cpu, ret ret);

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

        template <typename pointee>
        void write_return_value(arm::jitter &cpu, const ptr<pointee> &ret) {
            write_return_value(cpu, ret.ptr_address());
        }
    }
}