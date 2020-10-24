/*
 * Copyright (c) 2018 EKA2L1 Team.
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

#include <capstone/capstone.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1 {
    class memory_system;

    class disasm {
        using insn_ptr = std::unique_ptr<cs_insn, std::function<void(cs_insn *)>>;
        using csh_ptr = std::unique_ptr<csh, std::function<void(csh *)>>;

        csh cp_handle;
        insn_ptr cp_insn;

    public:
        explicit disasm();
        ~disasm();

        /**
         * @brief Disassemble binary code.
		 * @returns The description of the instruction.
		*/
        std::string disassemble(const uint8_t *code, size_t size, uint64_t address, bool thumb);
    };
}
