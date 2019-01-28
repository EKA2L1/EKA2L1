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

namespace eka2l1::arm {
    void arm_analyser::analyse(vaddress addr, std::vector<arm_function> funcs) {
        bool should_stop = false;
        bool thumb = addr & 1;

        arm_function func;

        const vaddress org_addr = addr;
        std::size_t boffset = 0;
        std::size_t bsize = 0;

        std::function<void(vaddress, std::size_t, std::size_t)> do_block;
        do_block = [&](vaddress start_addr, std::size_t crr_block_offset, std::size_t crr_block_size) -> void {
            while (true) {
                // Identify block and function
                auto inst = next_instruction(start_addr);

                if (!inst) {
                    return;
                }

                // Check if the PC is directly written
                std::vector<arm::reg> written_regs = inst->get_regs_write();
                if (std::find(written_regs.begin(), written_regs.end(), arm::reg::R15) == written_regs.end()) {
                    // Conclude our function
                    // Great show, aahhahaha
                    should_stop = true;
                    start_addr += inst->size;

                    func.size = start_addr - org_addr;

                    return;
                }

                // No ? We should find out if it's branching.
                // Relatively ? A new block
                // Directly ? A new function owo
                if (inst->group & arm_instruction_group::group_branch) {
                    // BX LR. This is usually a pattern that ends a function.
                    if (inst->opcode == 0x1EFF2FE1 || inst->opcode16 == 0x7047) {
                        should_stop = true;
                        start_addr += inst->size;

                        func.size = start_addr - org_addr;

                        return;
                    }
                    
                    // Do the next block first
                    do_block(addr + inst->size, crr_block_offset + inst->size, 0);
                    
                    if (inst->group & arm_instruction_group::group_branch_relative) {
                        func.blocks.emplace(crr_block_offset, crr_block_size);
                        crr_block_size = 0;

                        // TODO
                        const int32_t imm = inst->ops[0].imm;

                        if (imm < 0) {
                            const std::size_t to_branch_addr = crr_block_offset + imm;

                            // Pass 1: Find this block address right away
                            // If it exists, we can ignore this branch, else
                            if (func.blocks.find(to_branch_addr) == func.blocks.end()) {
                                // Pass 2: Find this branch address maybe in a block.
                                //
                                // It is traversing somewhere, maybe back in some block.
                                // We need to find the correspond block, divide to small parts
                                // it's not like... Im cutting meatss bk
                                bool found = false;
                                std::size_t new_block_size = 0;

                                for (auto &[offset, size]: func.blocks) {
                                    if (offset < to_branch_addr && to_branch_addr < offset + size) {
                                        found = true;

                                        // Shorten the size of this block
                                        new_block_size = offset + size - to_branch_addr;
                                        size = to_branch_addr - offset;

                                        break;
                                    }
                                }

                                if (found) {
                                    func.blocks.emplace(to_branch_addr, new_block_size);
                                }
                            }
                        } else {
                            // Lift!
                            start_addr += imm + (thumb ? 4 : 8);
                            crr_block_offset += (thumb ? 4 : 8);

                            addr = start_addr;
                            boffset = crr_block_offset;
                            bsize = 0;

                            break;
                        }
                    } else {
                        if (inst->ops[0].type == op_imm) {
                            analyse(inst->ops[0].imm, funcs);
                        }
                    }
                }

                // At least we should do something
                start_addr += inst->size;
                crr_block_size += inst->size;
            }
        };

        while (!should_stop) {
            do_block(addr, boffset, bsize);
        }

        funcs.push_back(func);
    }
}