/*
    armvfp.c - ARM VFPv3 emulation unit
    Copyright (C) 2003 Skyeye Develop Group
    for help please send mail to <skyeye-developer@lists.gro.clinux.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Note: this file handles interface with arm core and vfp registers */

#include <common/log.h>
#include <common/types.h>
#include <cpu/dyncom/armstate.h>
#include <cpu/dyncom/vfp/asm_vfp.h>
#include <cpu/dyncom/vfp/vfp.h>

void VFPInit(ARMul_State* state) {
    state->VFP[VFP_FPSID] = VFP_FPSID_IMPLMEN << 24 | VFP_FPSID_SW << 23 | VFP_FPSID_SUBARCH << 16 |
                            VFP_FPSID_PARTNUM << 8 | VFP_FPSID_VARIANT << 4 | VFP_FPSID_REVISION;
    state->VFP[VFP_FPEXC] = 0;
    state->VFP[VFP_FPSCR] = 0;

    // ARM11 MPCore instruction register reset values.
    state->VFP[VFP_FPINST] = 0xEE000A00;
    state->VFP[VFP_FPINST2] = 0;

    // ARM11 MPCore feature register values.
    state->VFP[VFP_MVFR0] = 0x11111111;
    state->VFP[VFP_MVFR1] = 0;
}

void VMOVBRS(ARMul_State* state, std::uint32_t to_arm, std::uint32_t t, std::uint32_t n, std::uint32_t *value) {
    if (to_arm) {
        *value = state->ExtReg[n];
    } else {
        state->ExtReg[n] = *value;
    }
}

void VMOVBRRD(ARMul_State* state, std::uint32_t to_arm, std::uint32_t t, std::uint32_t t2, std::uint32_t n, std::uint32_t *value1, std::uint32_t *value2) {
    if (to_arm) {
        *value2 = state->ExtReg[n * 2 + 1];
        *value1 = state->ExtReg[n * 2];
    } else {
        state->ExtReg[n * 2 + 1] = *value2;
        state->ExtReg[n * 2] = *value1;
    }
}
void VMOVBRRSS(ARMul_State* state, std::uint32_t to_arm, std::uint32_t t, std::uint32_t t2, std::uint32_t n, std::uint32_t *value1, std::uint32_t *value2) {
    if (to_arm) {
        *value1 = state->ExtReg[n + 0];
        *value2 = state->ExtReg[n + 1];
    } else {
        state->ExtReg[n + 0] = *value1;
        state->ExtReg[n + 1] = *value2;
    }
}

void VMOVI(ARMul_State* state, std::uint32_t single, std::uint32_t d, std::uint32_t imm) {
    if (single) {
        state->ExtReg[d] = imm;
    } else {
        /* Check endian please */
        state->ExtReg[d * 2 + 1] = imm;
        state->ExtReg[d * 2] = 0;
    }
}
void VMOVR(ARMul_State* state, std::uint32_t single, std::uint32_t d, std::uint32_t m) {
    if (single) {
        state->ExtReg[d] = state->ExtReg[m];
    } else {
        /* Check endian please */
        state->ExtReg[d * 2 + 1] = state->ExtReg[m * 2 + 1];
        state->ExtReg[d * 2] = state->ExtReg[m * 2];
    }
}

/* Miscellaneous functions */
std::int32_t  vfp_get_float(ARMul_State* state, unsigned int reg) {
    LOG_TRACE(eka2l1::CPU_DYNCOM, "VFP get float: s{}=[{:08x}]", reg, state->ExtReg[reg]);
    return state->ExtReg[reg];
}

void vfp_put_float(ARMul_State* state, std::int32_t  val, unsigned int reg) {
    LOG_TRACE(eka2l1::CPU_DYNCOM, "VFP put float: s{} <= [{:08x}]", reg, val);
    state->ExtReg[reg] = val;
}

std::uint64_t  vfp_get_double(ARMul_State* state, unsigned int reg) {
    std::uint64_t  result = ((std::uint64_t)state->ExtReg[reg * 2 + 1]) << 32 | state->ExtReg[reg * 2];
    LOG_TRACE(eka2l1::CPU_DYNCOM, "VFP get double: s[{}-{}]=[{:016llx}]", reg * 2 + 1, reg * 2, result);
    return result;
}

void vfp_put_double(ARMul_State* state, std::uint64_t  val, unsigned int reg) {
    LOG_TRACE(eka2l1::CPU_DYNCOM, "VFP put double: s[{}-{}] <= [{:08x}-{:08x}]", reg * 2 + 1, reg * 2,
              (std::uint32_t)(val >> 32), (std::uint32_t)(val & 0xffffffff));
    state->ExtReg[reg * 2] = (std::uint32_t)(val & 0xffffffff);
    state->ExtReg[reg * 2 + 1] = (std::uint32_t)(val >> 32);
}

/*
 * Process bitmask of exception conditions. (from vfpmodule.c)
 */
void vfp_raise_exceptions(ARMul_State* state, std::uint32_t exceptions, std::uint32_t inst, std::uint32_t fpscr) {
    LOG_TRACE(eka2l1::CPU_DYNCOM, "VFP: raising exceptions {:08x}", exceptions);

    if (exceptions == VFP_EXCEPTION_ERROR) {
        LOG_CRITICAL(eka2l1::CPU_DYNCOM, "unhandled bounce {:x}", inst);
        //Crash();
    }

    /*
     * If any of the status flags are set, update the FPSCR.
     * Comparison instructions always return at least one of
     * these flags set.
     */
    if (exceptions & (FPSCR_NFLAG | FPSCR_ZFLAG | FPSCR_CFLAG | FPSCR_VFLAG))
        fpscr &= ~(FPSCR_NFLAG | FPSCR_ZFLAG | FPSCR_CFLAG | FPSCR_VFLAG);

    fpscr |= exceptions;

    state->VFP[VFP_FPSCR] = fpscr;
}
