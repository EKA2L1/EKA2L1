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

#include <cstdint>
#include <memory>
#include <vector>

namespace eka2l1 {
    enum arm_reg {
        R0,
        R1,
        R2,
        R3,
        R4,
        R5,
        R6,
        R7,
        R8,
        R9,
        R10,
        R11,
        R12,
        R13,
        R14,
        R15,
        // TODO: VFP NEON registers
        SP = R13,
        LR = R14,
        PC = R15
    };

    enum class arm_disassembler_backend {
        capstone = 0,       ///< Using capstone to analyse and disassemble instruction.
        homemade = 2        ///< Using homemade decoder. Not really smart
    };

    enum class arm_instruction_type {
        arm,
        thumb16,
        thumb32
    };

    struct arm_instruction_base {
        union {
            // For thumb32 and arm
            std::uint32_t opcode;
            std::uint16_t opcode16;
        };
        
        std::uint8_t  size;

        virtual ~arm_instruction_base() {}
        
        /*! \brief Get the disassembled result in text format.
        */
        virtual std::string mnemonic() = 0;

        /*! \brief Get all the registers which are being read by this instruction.
         *
         * \returns A vector contains all registers suited.
        */
        virtual std::vector<arm_reg> get_regs_read() = 0;
        
        /*! \brief Get all the registers which are being written by this instruction.
         *
         * \returns A vector contains all registers suited.
        */
        virtual std::vector<arm_reg> get_regs_write() = 0;
    };

    class arm_analyser {
        std::uint8_t *code;
        std::size_t   size;

        /* False = Thumb, True = ARM
        */
        bool          mode;
    public:
        explicit arm_analyser()
            : code(nullptr), size(0), mode(false) {
        }

        /*! \brief Set the code to analyse.
         *
         * \param asize Size of the code block to analyse. 0 means infinite analyse.
         * \param acode Pointer to the code.
         * \param thumb Set mode to analyse
         * 
         * \returns True if set succeed.
        */
        bool set_analyse_start(std::uint8_t *acode, const std::size_t asize, const bool thumb);
        
        /*! \brief Get the next instruction disassembled.
         *
         * If the analyser reaches the limit, it will return a nullptr.
        */
        virtual std::shared_ptr<arm_instruction_base> next_instruction() {
            return nullptr;
        }
    };
}