#include <arm/arm_analyser.h>

namespace eka2l1::arm {
    void arm_analyser::analyse(vaddress addr, std::vector<arm_function> funcs) {
        bool should_stop = false;
        bool thumb = addr & 1;

        arm_function func;

        const vaddress org_addr = addr;
        std::size_t crr_block_offset = 0;
        std::size_t crr_block_size = 0;

        while (!should_stop) {
            std::function<void(vaddress, std::size_t, std::size_t)> do_block;
            do_block = [&](vaddress start_addr, std::size_t crr_block_offset, std::size_t crr_block_size) -> void {
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
                    addr += inst->size;

                    func.size = addr - org_addr;

                    return;
                }

                // No ? We should find out if it's branching.
                // Relatively ? A new block
                // Directly ? A new function owo
                if (inst->group & arm_instruction_group::group_branch) {
                    // BX LR. This is usually a pattern that ends a function.
                    if (inst->opcode == 0x1EFF2FE1 || inst->opcode16 == 0x7047) {
                        should_stop = true;
                        addr += inst->size;

                        return;
                    }
                    
                    // Check if the branch is uncondition
                    // If it's not, we may want to divide it to two ways
                    // One is continue, one is jumping to that block/function
                    // Either way, it should be fun
                    if (inst->cond != arm::cc::AL || 
                        inst->iname == instruction::BL || inst->iname == instruction::BLX) {
                        do_block(addr + inst->size, crr_block_offset + inst->size, 0);
                    }
                    
                    if (inst->group & arm_instruction_group::group_branch_relative) {
                        func.blocks.emplace(crr_block_offset, crr_block_size);
                        crr_block_size = 0;

                        // TODO
                        start_addr = addr + 0;
                        crr_block_offset = crr_block_offset + 0 + 8;
                    } else {
                        analyse(0, funcs);
                    }
                }

                // At least we should do something
                start_addr += inst->size;
                crr_block_size += inst->size;
            };

            do_block(addr, crr_block_offset, crr_block_size);
        }

        funcs.push_back(func);
    }
}