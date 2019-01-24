#pragma once

#include <arm/arm_analyser.h>
#include <cstdint>

struct cs_insn;
typedef size_t csh;

namespace eka2l1::arm {
    struct arm_instruction_capstone: public arm::arm_instruction_base {
        cs_insn *insn;

    public:
        ~arm_instruction_capstone() override {
        }

        std::string mnemonic() override;
        std::vector<arm::reg> get_regs_read() override;
        std::vector<arm::reg> get_regs_write() override;
    };

    class arm_analyser_capstone: public arm::arm_analyser {
        csh cp_handle_arm;
        csh cp_handle_thumb;

        cs_insn *insns_arm;
        cs_insn *insns_thumb;

    public:
        explicit arm_analyser_capstone();
        ~arm_analyser_capstone();
    };
}