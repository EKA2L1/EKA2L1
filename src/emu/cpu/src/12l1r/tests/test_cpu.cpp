#include "test_cpu.h"

namespace eka2l1::arm {
    test_env::test_env()
        : monitor_(1) {
    }

    test_env_tlb::test_env_tlb(const std::uint32_t mem_size)
        : pages_((mem_size + ENV_PAGE_SIZE - 1) >> ENV_PAGE_BITS)
        , monitor_(1) {
        for (std::uint32_t i = 0; i < pages_.size(); i++) {
            pages_[i].resize(ENV_PAGE_SIZE);
        }
    }

    void test_env_tlb::fill_tlb(r12l1_core &core) {
        for (std::uint32_t i = 0; i < pages_.size(); i++) {
            core.set_tlb_page(i * ENV_PAGE_SIZE, pages_[i].data(), prot_read_write);
        }
    }

    std::uint8_t *test_env_tlb::pointer(const std::uint32_t addr) {
        if ((addr >> ENV_PAGE_BITS) >= pages_.size()) {
            return nullptr;
        }

        std::uint8_t *result = pages_[addr >> ENV_PAGE_BITS].data() + (addr % ENV_PAGE_SIZE);
        return result;
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

    std::unique_ptr<r12l1_core> make_test_cpu(test_env_tlb &environment) {
        std::unique_ptr<r12l1_core> core = std::make_unique<r12l1_core>(&environment.monitor_, test_env_tlb::ENV_PAGE_BITS);
        test_env_tlb *environment_ptr = &environment;
        r12l1_core *core_ptr = core.get();

        core->read_code = [environment_ptr](const address addr, std::uint32_t *result) {
            std::uint32_t *data = reinterpret_cast<std::uint32_t*>(
                    reinterpret_cast<std::uint8_t*>(environment_ptr->code_.data()) + addr);

            *result = *data;
            return true;
        };

        core->read_8bit =  [environment_ptr, core_ptr](const address addr, std::uint8_t *result) {
            std::uint8_t *data_ptr = environment_ptr->pointer(addr);
            if (!data_ptr) {
                return false;
            }
            *result = *data_ptr;
            core_ptr->set_tlb_page(addr, data_ptr, prot_read_write);
            return true;
        };

        core->write_8bit =  [environment_ptr, core_ptr](const address addr, std::uint8_t *result) {
            std::uint8_t *data_ptr = environment_ptr->pointer(addr);
            if (!data_ptr) {
                return false;
            }
            *data_ptr = *result;
            core_ptr->set_tlb_page(addr, data_ptr, prot_read_write);
            return true;
        };

        core->read_16bit =  [environment_ptr, core_ptr](const address addr, std::uint16_t *result) {
            std::uint16_t *data_ptr = reinterpret_cast<std::uint16_t*>(environment_ptr->pointer(addr));
            if (!data_ptr) {
                return false;
            }
            *result = *data_ptr;
            core_ptr->set_tlb_page(addr, reinterpret_cast<std::uint8_t*>(data_ptr), prot_read_write);
            return true;
        };

        core->write_16bit =  [environment_ptr, core_ptr](const address addr, std::uint16_t *result) {
            std::uint16_t *data_ptr = reinterpret_cast<std::uint16_t*>(environment_ptr->pointer(addr));
            if (!data_ptr) {
                return false;
            }
            *data_ptr = *result;
            core_ptr->set_tlb_page(addr, reinterpret_cast<std::uint8_t*>(data_ptr), prot_read_write);
            return true;
        };

        core->read_32bit =  [environment_ptr, core_ptr](const address addr, std::uint32_t *result) {
            std::uint32_t *data_ptr = reinterpret_cast<std::uint32_t*>(environment_ptr->pointer(addr));
            if (!data_ptr) {
                return false;
            }
            *result = *data_ptr;
            core_ptr->set_tlb_page(addr, reinterpret_cast<std::uint8_t*>(data_ptr), prot_read_write);
            return true;
        };

        core->write_32bit =  [environment_ptr, core_ptr](const address addr, std::uint32_t *result) {
            std::uint32_t *data_ptr = reinterpret_cast<std::uint32_t*>(environment_ptr->pointer(addr));
            if (!data_ptr) {
                return false;
            }
            *data_ptr = *result;
            core_ptr->set_tlb_page(addr, reinterpret_cast<std::uint8_t*>(data_ptr), prot_read_write);
            return true;
        };

        core->read_64bit =  [environment_ptr, core_ptr](const address addr, std::uint64_t *result) {
            std::uint64_t *data_ptr = reinterpret_cast<std::uint64_t*>(environment_ptr->pointer(addr));
            if (!data_ptr) {
                return false;
            }
            *result = *data_ptr;
            core_ptr->set_tlb_page(addr, reinterpret_cast<std::uint8_t*>(data_ptr), prot_read_write);
            return true;
        };

        core->write_64bit =  [environment_ptr, core_ptr](const address addr, std::uint64_t *result) {
            std::uint64_t *data_ptr = reinterpret_cast<std::uint64_t*>(environment_ptr->pointer(addr));
            if (!data_ptr) {
                return false;
            }
            *data_ptr = *result;
            core_ptr->set_tlb_page(addr, reinterpret_cast<std::uint8_t*>(data_ptr), prot_read_write);
            return true;
        };

        return core;
    }
}