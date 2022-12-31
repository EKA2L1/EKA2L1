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

#include <common/log.h>
#include <common/types.h>

#include <capstone/capstone.h>
#include <disasm/disasm.h>

#include <functional>
#include <memory>
#include <sstream>

namespace eka2l1 {
    void shutdown_insn(cs_insn *insn) {
        if (insn) {
            cs_free(insn, 1);
        }
    }

    disasm::disasm() {
        cs_err err = cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &cp_handle);

        if (err != CS_ERR_OK) {
            LOG_ERROR(DISASM, "Capstone open errored! Error code: {}", static_cast<int>(err));
            return;
        }

        cp_insn = insn_ptr(cs_malloc(cp_handle), shutdown_insn);
        cs_option(cp_handle, CS_OPT_SKIPDATA, CS_OPT_ON);

        if (!cp_insn) {
            LOG_ERROR(DISASM, "Capstone INSN allocation failed!");
            return;
        }
    }

    disasm::~disasm() {
        cp_insn.reset();
        cs_close(&cp_handle);
    }

    std::string disasm::disassemble(const uint8_t *code, size_t size, uint64_t address, bool thumb) {
        cs_err err = cs_option(cp_handle, CS_OPT_MODE, thumb ? CS_MODE_THUMB : CS_MODE_ARM);

        if (err != CS_ERR_OK) {
            LOG_ERROR(DISASM, "Unable to set disassemble option! Error: {}", err);
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
}
