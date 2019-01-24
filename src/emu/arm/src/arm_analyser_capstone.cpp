#include <arm/arm_analyser_capstone.h>
#include <capstone/capstone.h>

#include <common/log.h>

namespace eka2l1::arm {
    std::string arm_instruction_capstone::mnemonic() {
        return insn->mnemonic;
    }

    std::vector<arm::reg> arm_instruction_capstone::get_regs_read() {
        std::vector<arm::reg> regs;
        regs.resize(insn->detail->regs_read_count);

        for (std::size_t i = 0; i < regs.size(); i++) {
            regs[i] = static_cast<arm::reg>(insn->detail->regs_read[i]);
        }

        return regs;
    }
    
    std::vector<arm::reg> arm_instruction_capstone::get_regs_write() {
        std::vector<arm::reg> regs;
        regs.resize(insn->detail->regs_write_count);

        for (std::size_t i = 0; i < regs.size(); i++) {
            regs[i] = static_cast<arm::reg>(insn->detail->regs_write[i]);
        }

        return regs;
    }

    arm_analyser_capstone::arm_analyser_capstone() {
        cs_err err = cs_open(CS_ARCH_ARM, CS_MODE_ARM, &cp_handle_arm);

        if (err != CS_ERR_OK) {
            LOG_ERROR("ARM analyser handle can't be initialized");
        }
        
        err = cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &cp_handle_thumb);

        if (err != CS_ERR_OK) {
            LOG_ERROR("THUMB analyser handle can't be initialized");
        }

        insns_arm = cs_malloc(cp_handle_arm);
        insns_thumb = cs_malloc(cp_handle_thumb);
    }

    arm_analyser_capstone::~arm_analyser_capstone() {
        cs_free(insns_arm, 1);
        cs_free(insns_thumb, 1);

        cs_close(&cp_handle_arm);
        cs_close(&cp_handle_thumb);
    }
}