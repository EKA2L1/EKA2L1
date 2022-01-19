#include "test_cpu.h"

namespace eka2l1::arm {
    test_env::test_env()
        : monitor_(1) {
    }

    std::unique_ptr<r12l1_core> make_test_cpu(test_env &environment) {
        std::unique_ptr<r12l1_core> core = std::make_unique<r12l1_core>(&environment.monitor_, 12);
        test_env *environment_ptr = &environment;

        core->set_reg(13, static_cast<std::uint32_t>(environment.stack_.size()));

        core->read_code = [environment_ptr](const address addr, std::uint32_t *result) {
            std::uint32_t *data = reinterpret_cast<std::uint32_t*>(
                    reinterpret_cast<std::uint8_t*>(environment_ptr->code_.data()) + addr);

            *result = *data;
            return true;
        };

        core->read_8bit =  [environment_ptr](const address addr, std::uint8_t *result) {
            *result = *(reinterpret_cast<std::uint8_t*>(environment_ptr->code_.data()) + addr);
            return true;
        };

        core->write_8bit =  [environment_ptr](const address addr, std::uint8_t *result) {
            *(reinterpret_cast<std::uint8_t*>(environment_ptr->stack_.data()) + addr) = *result;
            return true;
        };

        core->read_16bit =  [environment_ptr](const address addr, std::uint16_t *result) {
            *result = *reinterpret_cast<std::uint16_t*>
                (reinterpret_cast<std::uint8_t*>(environment_ptr->code_.data()) + addr);

            return true;
        };

        core->write_16bit =  [environment_ptr](const address addr, std::uint16_t *result) {
            *(reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uint8_t*>(environment_ptr->stack_.data()) + addr)) = *result;
            return true;
        };

        core->read_32bit =  [environment_ptr](const address addr, std::uint32_t *result) {
            *result = *reinterpret_cast<std::uint32_t*>
                (reinterpret_cast<std::uint8_t*>(environment_ptr->code_.data()) + addr);

            return true;
        };

        core->write_32bit =  [environment_ptr](const address addr, std::uint32_t *result) {
            *(reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(environment_ptr->stack_.data()) + addr)) = *result;
            return true;
        };

        core->read_64bit =  [environment_ptr](const address addr, std::uint64_t *result) {
            *result = *reinterpret_cast<std::uint64_t*>
                (reinterpret_cast<std::uint8_t*>(environment_ptr->code_.data()) + addr);

            return true;
        };

        core->write_64bit =  [environment_ptr](const address addr, std::uint64_t *result) {
            *(reinterpret_cast<std::uint64_t*>(
                    reinterpret_cast<std::uint8_t*>(environment_ptr->stack_.data()) + addr)) = *result;
            return true;
        };

        return core;
    }
}