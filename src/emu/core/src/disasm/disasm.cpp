#include <common/log.h>
#include <common/types.h>
#include <arm/jit_factory.h>
#include <core_mem.h>
#include <capstone.h>
#include <disasm/disasm.h>
#include <ptr.h>

#include <functional>
#include <memory>
#include <sstream>

namespace eka2l1 {
    void shutdown_insn(cs_insn *insn) {
        if (insn) {
            cs_free(insn, 1);
        }
    }

    void disasm::init(memory* smem) {
        cs_err err = cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &cp_handle);

        if (err != CS_ERR_OK) {
            LOG_ERROR("Capstone open errored! Error code: {}", err);
            return;
        }

        cp_insn = insn_ptr(cs_malloc(cp_handle), shutdown_insn);
        cs_option(cp_handle, CS_OPT_SKIPDATA, CS_OPT_ON);

        if (!cp_insn) {
            LOG_ERROR("Capstone INSN allocation failed!");
            return;
        }

        mem = smem;

        // Create jitter to detect thumb
        jitter = arm::create_jitter(nullptr, mem, this, arm::jitter_arm_type::unicorn);
    }

    void disasm::shutdown() {
        cp_insn.reset();
        cs_close(&cp_handle);
    }

    std::string disasm::disassemble(const uint8_t *code, size_t size, uint64_t address, bool thumb) {
        cs_err err = cs_option(cp_handle, CS_OPT_MODE, thumb ? CS_MODE_THUMB : CS_MODE_ARM);

        if (err != CS_ERR_OK) {
            LOG_ERROR("Unable to set disassemble option! Error: {}", err);
            return "";
        }

        bool success = cs_disasm_iter(cp_handle, &code, &size, &address, cp_insn.get());

        std::ostringstream out;
        out << cp_insn->mnemonic << " " << cp_insn->op_str;

        if (!success) {
            cs_err err = cs_errno(cp_handle);
            out << " (" << cs_strerror(err) << ")";
        }

        return out.str();
    }

    subroutine disasm::get_subroutine(ptr<uint8_t> beg) {
        address beg_addr = beg.ptr_address();
        address progressed = 0;

        subroutine sub;

        jitter->set_pc(beg_addr);

        bool found = false;

        while (!found) {
            bool thumb = jitter->is_thumb_mode();
            size_t inst_size = thumb ? 2 : 4;

            std::string inst = disassemble(mem->get_addr<uint8_t>(beg_addr + progressed), inst_size,
                0, thumb);

            if (FOUND_STR(inst.find("("))) {
                for (uint8_t i = 0; i < inst_size; i++) {
                    sub.insts.push_back(arm_inst_type::data);
                }
            } else {
                sub.insts.push_back(thumb ? arm_inst_type::thumb : arm_inst_type::arm);
            
                // Quick hack: if the pc is modified (means pc go first, have space),
                // or if inst is branch or jump, then subroutine is here
                if (inst[0] == 'b' || inst[0] == 'j' || FOUND_STR(inst.find(" pc"))) {
                    sub.end = ptr<uint8_t>(beg_addr + progressed);
                    found = true;
                }
            }

            progressed += inst_size;
            // Step the jit
            jitter->step();
        }

        return sub;
    }
}
