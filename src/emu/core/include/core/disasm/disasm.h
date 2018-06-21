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

#include <capstone.h>
#include <memory>
#include <ptr.h>
#include <string>
#include <vector>

namespace eka2l1 {
	/*! \brief Contains ARM jitter and related things. */
    namespace arm {
        class jit_interface;
        using jitter = std::unique_ptr<jit_interface>;
    }

    template <class T>
    class ptr;

    class memory;

    enum class arm_inst_type {
        arm = 0x00,
        thumb = 0x01,
        data = 0x02
    };

	/*! \brief Represents a subroutine. */
    struct subroutine {
        std::vector<arm_inst_type> insts;
        ptr<uint8_t> end;
    };

	/*! \brief An ARM disassembler. */
    class disasm {
        using insn_ptr = std::unique_ptr<cs_insn, std::function<void(cs_insn *)>>;
        using csh_ptr = std::unique_ptr<csh, std::function<void(csh *)>>;

        csh cp_handle;
        insn_ptr cp_insn;
        memory_system *mem;

        // This jitter is use for detect subroutine.
        arm::jitter jitter;

    public:
	    /*! \brief Initialize the disassembler.
		 * \param smem The memory system.
		*/
        void init(memory_system *smem);
        
		/*! \brief Shutdown the disassembler. */
		void shutdown();

		/*! \brief Get a subroutine.
		 * \param beg HLE pointer to the start of a subroutine.
		 * \returns The subroutine
		*/
        subroutine get_subroutine(ptr<uint8_t> beg);
		
		/*! \brief Disassemble binary code.
		 * \returns The description of the instruction.
		*/
        std::string disassemble(const uint8_t *code, size_t size, uint64_t address, bool thumb);
    };
}

