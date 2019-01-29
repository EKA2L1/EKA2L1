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
#include <queue>

namespace eka2l1::arm {
     std::vector<arm_function> arm_analyser::analyse(vaddress addr, vaddress limit) {
        bool should_stop = false;
        bool thumb = addr & 1;

        std::vector<arm_function> funcs;
        // use BFS since DFS is hard to see what's wrong
        std::queue<arm_function*> funcs_queue;

        auto add_func = [&](vaddress faddr) {
            funcs.push_back(arm_function(faddr, 0));
            funcs_queue.push(&funcs.back());
        };

        // Add intial function
        add_func(addr);

        for (auto func = funcs_queue.front(); !funcs_queue.empty(); ) {
            std::queue<std::pair<const vaddress, std::size_t>*> blocks_queue;

            auto add_block = [&](vaddress block_addr) {
                auto pair = func->blocks.emplace(block_addr, 0);
                if (pair.second) {
                    blocks_queue.push(&(*pair.first));
                }
            };

            // Initial block
            add_block(addr);

            for (auto block = blocks_queue.front(); !blocks_queue.empty(); ) {
                bool should_stop = false;
                for (auto baddr = block->first; baddr <= limit, should_stop; ) {
                    auto inst = next_instruction(baddr);

                    switch (inst->iname) {
                    // End a block
                    case instruction::B: case instruction::BL: case instruction::BX:
                    case instruction::BLX: {
                        add_block(inst->ops[0].imm);
                        block->second = baddr - block->first;
                        should_stop = true;

                        break;
                    }

                    case instruction::POP: case instruction::LDM: {
                        // Check for last op
                        if (inst->ops.back().type == op_reg && inst->ops.back().reg == arm::reg::R15) {
                            block->second = baddr - block->first;
                            should_stop = true;
                        }

                        break;
                    }

                    case instruction::LDR: {
                        if (inst->ops.front().type == op_reg && inst->ops.front().reg == arm::reg::R15) {
                            block->second = baddr - block->first;
                            should_stop = true;
                        }

                        break;
                    }

                    default: {
                        break;
                    }
                    }

                    baddr += inst->size;
                }
            }
        }

        return funcs;
    }
}