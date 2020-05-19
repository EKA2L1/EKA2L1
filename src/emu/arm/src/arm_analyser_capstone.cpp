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

#include <arm/arm_analyser_capstone.h>
#include <capstone/capstone.h>

#include <common/log.h>

namespace eka2l1::arm {

    /*! \brief Convert a capstone reg to our register presentation.
    */
    static arm::reg capstone_reg_to_my_reg(int reg) {
        if (reg >= ARM_REG_R0 && reg <= ARM_REG_R0 + 15) {
            return static_cast<arm::reg>(reg - ARM_REG_R0 + arm::R0);
        }

        switch (reg) {
        case ARM_REG_PC: {
            return arm::R15;
        }

        case ARM_REG_LR: {
            return arm::R14;
        }

        case ARM_REG_SP: {
            return arm::R13;
        }

        default: {
            // TODO
            break;
        }
        }

        return arm::INVALID;
    }

    std::string arm_instruction_capstone::mnemonic() {
        return insn->mnemonic;
    }

    std::vector<arm::reg> arm_instruction_capstone::get_regs_read() {
        std::vector<arm::reg> regs;
        regs.resize(insn->detail->regs_read_count);

        for (std::size_t i = 0; i < regs.size(); i++) {
            regs[i] = capstone_reg_to_my_reg(insn->detail->regs_read[i]);
        }

        return regs;
    }

    std::vector<arm::reg> arm_instruction_capstone::get_regs_write() {
        std::vector<arm::reg> regs;
        regs.resize(insn->detail->regs_write_count);

        for (std::size_t i = 0; i < regs.size(); i++) {
            regs[i] = capstone_reg_to_my_reg(insn->detail->regs_write[i]);
        }

        return regs;
    }

    arm_analyser_capstone::arm_analyser_capstone(std::function<std::uint32_t(vaddress)> read_func)
        : arm_analyser(read_func) {
        cs_err err = cs_open(CS_ARCH_ARM, CS_MODE_ARM, &cp_handle_arm);

        if (err != CS_ERR_OK) {
            LOG_ERROR("ARM analyser handle can't be initialized");
        }

        err = cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &cp_handle_thumb);

        cs_option(cp_handle_arm, CS_OPT_DETAIL, CS_OPT_ON);
        cs_option(cp_handle_thumb, CS_OPT_DETAIL, CS_OPT_ON);

        if (err != CS_ERR_OK) {
            LOG_ERROR("THUMB analyser handle can't be initialized");
        }

        insns = cs_malloc(cp_handle_thumb);
    }

    arm_analyser_capstone::~arm_analyser_capstone() {
        cs_free(insns, 1);

        cs_close(&cp_handle_arm);
        cs_close(&cp_handle_thumb);
    }

    std::shared_ptr<arm_instruction_base> arm_analyser_capstone::next_instruction(vaddress addr) {
        const bool thumb = addr & 1;
        addr &= ~0x1;

        const std::uint32_t code = read(addr);
        std::size_t total_pass = 0;

        if (thumb) {
            total_pass = cs_disasm(cp_handle_thumb, reinterpret_cast<const std::uint8_t *>(&code), 4, addr, 1,
                &insns);
        } else {
            total_pass = cs_disasm(cp_handle_arm, reinterpret_cast<const std::uint8_t *>(&code), 4, addr, 1,
                &insns);
        }

        if (total_pass == 0) {
            // Silent
            // LOG_ERROR("Can't disassemble {} inst with given addr: 0x{:X}", thumb ? "thumb" : "arm", addr);
            return nullptr;
        }

        // Convert stuffs!
        std::shared_ptr<arm_instruction_capstone> il = std::make_shared<arm_instruction_capstone>(insns);

        if (thumb) {
            il->opcode16 = (code & 0xFF00);
        } else {
            il->opcode = code;
        }

        il->size = static_cast<std::uint8_t>(insns->size);
        il->iname = static_cast<arm::instruction>(insns->id);

        il->cond = static_cast<arm::cc>(insns->detail->arm.cc);

        std::uint8_t i = 0;

        // Copy directly all ops
        for (i = 0; i < insns->detail->arm.op_count; i++) {
            cs_arm_op *cs_op = &insns->detail->arm.operands[i];

            arm_op op;
            op.type = static_cast<decltype(op.type)>(cs_op->type);

            switch (op.type) {
            case op_imm: {
                op.imm = cs_op->imm;
                break;
            }
            case op_reg: {
                op.reg = capstone_reg_to_my_reg(cs_op->reg);
                break;
            }
            case op_mem: {
                std::memcpy(&op.mem, &cs_op->mem, sizeof(cs_op->mem));
                op.mem.base = capstone_reg_to_my_reg(op.mem.base);
                op.mem.index = capstone_reg_to_my_reg(op.mem.index);
                break;
            }
            default:
                break;
            }

            op.shift.type = static_cast<arm::shifter>(cs_op->shift.type);
            op.shift.value = cs_op->shift.value;

            il->ops.push_back(op);
        }

        il->group = 0;

        // Build group
        for (i = 0; i < insns->detail->groups_count; i++) {
            switch (insns->detail->groups[i]) {
            case ARM_GRP_JUMP: {
                il->group |= group_branch;
                break;
            }

            case ARM_GRP_BRANCH_RELATIVE: {
                il->group |= group_branch_relative;
                break;
            }

            case ARM_GRP_INT: {
                il->group |= group_interrupt;
                break;
            }

            // We are not gonna use those flags yet
            default:
                break;
            }
        }

        return il;
    }
}