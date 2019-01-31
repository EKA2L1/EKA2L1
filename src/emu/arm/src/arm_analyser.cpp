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
#include <queue>

#include <common/log.h>

namespace eka2l1::arm {
    void arm_analyser::analyse(std::unordered_map<vaddress, arm_function> &funcs, vaddress addr, vaddress limit) {
        bool should_stop = false;

        // use BFS since DFS is hard to see what's wrong
        std::queue<arm_function*> funcs_queue;

        auto add_func = [&](vaddress faddr) {
            const std::lock_guard<std::mutex> guard(lock);
            auto res = funcs.emplace(faddr, arm_function(faddr, 0));

            if (res.second) {
                funcs_queue.push(&(res.first->second));
            }
        };

        // Add intial function
        add_func(addr);

        if (funcs_queue.size() == 0) {
            return;
        }

        // Should
        for (auto func = funcs_queue.front(); !funcs_queue.empty(); ) {
            std::queue<std::pair<const vaddress, std::size_t>*> blocks_queue;

            auto add_block = [&](vaddress block_addr) {
                const std::lock_guard<std::mutex> guard(lock);

                auto pair = func->blocks.emplace(block_addr, 0);
                if (pair.second) {
                    blocks_queue.push(&(*pair.first));
                }
            };

            bool thumb = func->addr & 1;

            // Initial block
            add_block(func->addr);

            for (auto block = blocks_queue.front(); !blocks_queue.empty();) {
                bool should_stop = false;
                for (auto baddr = block->first; baddr <= limit && !should_stop; ) {
                    auto inst = next_instruction(baddr);

                    if (!inst) {
                        block->second = baddr - block->first;
                        should_stop = true;

                        break;
                    }

                    switch (inst->iname) {
                    // End a block
                    case instruction::B:  {
                        vaddress br = inst->ops[0].imm;
                        if (thumb) {
                            br |= 1;
                        }

                        add_block(br);

                        if (inst->cond == arm::cc::AL) {
                            block->second = baddr - block->first + inst->size;
                            should_stop = true;
                        }

                        break;
                    }

                    case instruction::BX: {
                        if (inst->ops[0].type == op_reg && inst->ops[0].reg == arm::reg::R12 && ip != 0) {                                 
                            // LOG_TRACE("Branching 0x{:X}, addr 0x{:X}", ip, baddr);

                            add_func(ip);
                            ip = 0;
                        }

                        block->second = baddr - block->first + inst->size;
                        should_stop = true;

                        break;
                    }

                    case instruction::BL: case instruction::BLX: {
                        if (inst->ops[0].type == op_imm) {
                            vaddress boff = inst->ops[0].imm;

                            if (instruction::BLX == inst->iname) {
                                boff &= ~0x1;

                                if (!thumb) {
                                    boff |= 1;
                                }
                            } else {
                                if (thumb) {
                                    boff |= 1;
                                }
                            }


                            // LOG_TRACE("Branching 0x{:X}, addr 0x{:X}", boff, baddr);
                            add_func(boff);
                        }

                        break;
                    }

                    case instruction::POP: case instruction::LDM: {
                        // Check for last op
                        if (inst->ops.back().type == op_reg && inst->ops.back().reg == arm::reg::R15) {
                            block->second = baddr - block->first + inst->size;
                            should_stop = true;
                        }

                        break;
                    }

                    // Fallthrough intentional
                    case instruction::LDR: {
                        if (inst->ops.front().type == op_reg && inst->ops.front().reg == arm::reg::R15) {
                            block->second = baddr - block->first + inst->size;
                            should_stop = true;
                        }
                        
                        // Detect trampoline
                        if (inst->ops.front().type == op_reg && inst->ops.front().reg == arm::reg::R12) {
                            // Using IP to jump
                            if (inst->ops[1].type == op_mem && inst->ops[1].mem.base == arm::reg::R15) {
                                ip = read(baddr + (thumb ? 4 : 8) + inst->ops[1].mem.disp);
                            }
                        }

                        break;
                    }

                    case instruction::ADD: {
                        if (inst->ops.front().type == op_reg && inst->ops.front().reg == arm::reg::R12) {
                            // Using IP to jump
                            if (inst->ops[1].type == op_reg && inst->ops[1].reg == arm::reg::R15) {
                                // Can assure op2 is imm
                                ip = baddr + (thumb ? 4 : 8) + inst->ops[2].imm;
                            }
                        }
                        
                        break;
                    }

                    default: {
                        break;
                    }
                    }

                    baddr += inst->size;
                }

                blocks_queue.pop();

                if (!blocks_queue.empty()) {
                    block = blocks_queue.front();
                }
            }

            // Calculate function size
            if (func->blocks.size() == 1) {
                // Only one block, don't waste time for those loop
                func->size = func->blocks.begin()->second;
            } else {
                // This is not the fastest way, but it works everytime                
                func->size = 0;
                vaddress cur_addr =  func->blocks.begin()->first;
                std::size_t cur_size = func->blocks.begin()->second;

                do {
                    if (cur_size == 0) {
                        break;
                    }
                    
                    func->size += cur_size;
                    auto res = func->blocks.find(static_cast<vaddress>(cur_addr + cur_size));

                    // TODO: Optimize
                    // Align bytes may pop up for aligment of branching
                    if (res == func->blocks.end()) {
                        res = func->blocks.find(static_cast<vaddress>(cur_addr + cur_size + (thumb ? 2 : 4)));
                    }

                    if (res != func->blocks.end()) {
                        cur_addr += static_cast<vaddress>(cur_size);
                        cur_size = res->second; 
                    } else {
                        break;
                    }
                } while (true);
            }

            // Next function in queue
            funcs_queue.pop();            

            if (!funcs_queue.empty()) {
                func = funcs_queue.front();
            } else {
                return;
            }
        }
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