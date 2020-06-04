/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#pragma once

#include <cpu/arm_analyser.h>
#include <cstdint>

struct cs_insn;
typedef size_t csh;

namespace eka2l1::arm {
    struct arm_instruction_capstone : public arm::arm_instruction_base {
        cs_insn *insn;

    public:
        explicit arm_instruction_capstone(cs_insn *insn)
            : insn(insn) {
        }

        ~arm_instruction_capstone() override {
        }

        std::string mnemonic() override;
        std::vector<arm::reg> get_regs_read() override;
        std::vector<arm::reg> get_regs_write() override;
    };

    class arm_analyser_capstone : public arm::arm_analyser {
        csh cp_handle_arm;
        csh cp_handle_thumb;

        cs_insn *insns;

    public:
        explicit arm_analyser_capstone(std::function<std::uint32_t(vaddress)> read_func);
        ~arm_analyser_capstone();

        std::shared_ptr<arm_instruction_base> next_instruction(vaddress addr) override;
    };
}