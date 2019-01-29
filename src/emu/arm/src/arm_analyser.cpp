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

#include <arm/arm_analyser.h>
#include <arm/arm_analyser_capstone.h>

#include <common/log.h>

namespace eka2l1::arm {
    void arm_analyser::analyse(vaddress addr, std::vector<arm_function> &funcs) {
        bool should_stop = false;
        bool thumb = addr & 1;

        arm_function func;

        const vaddress org_addr = addr;
        std::size_t boffset = 0;
        std::size_t bsize = 0;

        func.addr = addr;

        std::function<void(vaddress, std::size_t, std::size_t)> do_block;
        do_block = [&](vaddress start_addr, std::size_t crr_block_offset, std::size_t crr_block_size) -> void {
            while (true) {
                // Identify block and function
                auto inst = next_instruction(start_addr);

                LOG_TRACE("0x{:X} {}", start_addr, inst->mnemonic());

                if (!inst) {
                    return;
                }

                // We should find out if it's branching.
                // Relatively ? A new block
                // Directly ? A new function owo
                if (inst->group & arm_instruction_group::group_branch) {
                    // Branch unconditionaly exchange
                    if (inst->iname == instruction::BX) {
                        should_stop = true;
                        start_addr += inst->size;

                        func.size = start_addr - org_addr;

                        return;
                    }
                    
                    // BL and BLX are generally used for calling other subroutines
                    // B on normal hand (no AL), generally used for calling other blocks.
                    if (inst->iname != instruction::BL && inst->iname != instruction::BLX) {
                        func.blocks.emplace(crr_block_offset, crr_block_size);
                        crr_block_size = 0;

                        // Simply right now, don't iterate in, just emplace block
                    } else {
                        vaddress baddr = inst->ops[0].imm;

                        if (inst->iname == instruction::BLX) {
                            baddr &= ~0x1;
                            
                            // Toogle the mode
                            if (!thumb) {
                                baddr |= 0x1;
                            }
                        }

                        if (inst->ops[0].type == op_imm) {
                            analyse(baddr, funcs);
                        }
                    }
                } else {
                    bool do_end = false;
                    switch (inst->iname) {
                    // Pattern 1: LDM and POP (arm)
                    case instruction::LDM: case instruction::POP: {
                        // Symbian binary subroutine usually ends with a load including a PC
                        // Usually sits on the end of operand list iirc
                        if (inst->ops[inst->ops.size() - 1].type == op_reg && inst->ops[inst->ops.size() - 1].reg == arm::reg::R15) {
                            do_end = true;
                            break;
                        }

                        break;
                    }

                    case instruction::LDR: {
                        // Sometimes use for calling imports (most of the time)
                        if (inst->ops[0].type == op_reg && inst->ops[0].reg == arm::reg::R15) {
                            do_end = true;
                        }

                        break;
                    }

                    default:
                        break;
                    }

                    if (do_end) {
                        should_stop = true;
                        func.size = start_addr - org_addr + inst->size;
                        func.blocks.emplace(crr_block_offset, crr_block_size + inst->size);

                        return;
                    }
                }

                // At least we should do something
                start_addr += inst->size;
                crr_block_size += inst->size;
            }

            // Unreachable
            // func.blocks.emplace(crr_block_offset, crr_block_size);
        };

        while (!should_stop) {
            do_block(addr, boffset, bsize);
        }

        funcs.push_back(func);
    }
    
    std::unique_ptr<arm_analyser> make_analyser(const arm_disassembler_backend backend,
        read_code_func readf) {
        switch (backend) {
        case arm_disassembler_backend::capstone: {
            return std::make_unique<arm_analyser_capstone>(readf);
        }

        default: {
            break;
        }
        }

        return nullptr;
    }
}