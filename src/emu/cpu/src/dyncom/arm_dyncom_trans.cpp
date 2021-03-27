#include <cstdlib>
#include <cassert>
#include <common/types.h>
#include <common/log.h>
#include <cpu/dyncom/arm_dyncom_trans.h>
#include <cpu/dyncom/armstate.h>
#include <cpu/dyncom/armsupp.h>
#include <cpu/dyncom/vfp/vfp.h>

static void* AllocBuffer(ARMul_State *state, std::size_t size) {
    std::size_t start = state->trans_cache_buf_top;
    state->trans_cache_buf_top += size;
    assert(trans_cache_buf_top <= TRANS_CACHE_SIZE && "Translation cache is full!");
    return static_cast<void*>(&state->trans_cache_buf[start]);
}

#define glue(x, y) x##y
#define INTERPRETER_TRANSLATE(s) glue(InterpreterTranslate_, s)

shtop_fp_t GetShifterOp(unsigned int inst);
get_addr_fp_t GetAddressingOp(unsigned int inst);
get_addr_fp_t GetAddressingOpLoadStoreT(unsigned int inst);

static ARM_INST_PTR INTERPRETER_TRANSLATE(adc)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(adc_inst));
    adc_inst* inst_cream = (adc_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(add)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(add_inst));
    add_inst* inst_cream = (add_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(and)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(and_inst));
    and_inst* inst_cream = (and_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(bbl)(ARMul_State *state, unsigned int inst, int index) {
#define POSBRANCH ((inst & 0x7fffff) << 2)
#define NEGBRANCH ((0xff000000 | (inst & 0xffffff)) << 2)

    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(bbl_inst));
    bbl_inst* inst_cream = (bbl_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::DIRECT_BRANCH;

    if (BIT(inst, 24))
        inst_base->br = TransExtData::CALL;

    inst_cream->L = BIT(inst, 24);
    inst_cream->signed_immed_24 = BIT(inst, 23) ? NEGBRANCH : POSBRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(bic)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(bic_inst));
    bic_inst* inst_cream = (bic_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;
    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(bkpt)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(bkpt_inst));
    bkpt_inst* const inst_cream = (bkpt_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->imm = (BITS(inst, 8, 19) << 4) | BITS(inst, 0, 3);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(blx)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(blx_inst));
    blx_inst* inst_cream = (blx_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::INDIRECT_BRANCH;

    inst_cream->inst = inst;
    if (BITS(inst, 20, 27) == 0x12 && BITS(inst, 4, 7) == 0x3) {
        inst_cream->val.Rm = BITS(inst, 0, 3);
    } else {
        inst_cream->val.signed_immed_24 = BITS(inst, 0, 23);
    }

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(bx)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(bx_inst));
    bx_inst* inst_cream = (bx_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::INDIRECT_BRANCH;

    inst_cream->Rm = BITS(inst, 0, 3);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(bxj)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(bx)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(cdp)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(cdp_inst));
    cdp_inst* inst_cream = (cdp_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->CRm = BITS(inst, 0, 3);
    inst_cream->CRd = BITS(inst, 12, 15);
    inst_cream->CRn = BITS(inst, 16, 19);
    inst_cream->cp_num = BITS(inst, 8, 11);
    inst_cream->opcode_2 = BITS(inst, 5, 7);
    inst_cream->opcode_1 = BITS(inst, 20, 23);
    inst_cream->inst = inst;

    LOG_TRACE(eka2l1::CPU_DYNCOM, "inst {:x} index {:x}", inst, index);
    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(clrex)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(clrex_inst));
    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(clz)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(clz_inst));
    clz_inst* inst_cream = (clz_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rd = BITS(inst, 12, 15);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(cmn)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(cmn_inst));
    cmn_inst* inst_cream = (cmn_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(cmp)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(cmp_inst));
    cmp_inst* inst_cream = (cmp_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(cps)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(cps_inst));
    cps_inst* inst_cream = (cps_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->imod0 = BIT(inst, 18);
    inst_cream->imod1 = BIT(inst, 19);
    inst_cream->mmod = BIT(inst, 17);
    inst_cream->A = BIT(inst, 8);
    inst_cream->I = BIT(inst, 7);
    inst_cream->F = BIT(inst, 6);
    inst_cream->mode = BITS(inst, 0, 4);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(cpy)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(mov_inst));
    mov_inst* inst_cream = (mov_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15) {
        inst_base->br = TransExtData::INDIRECT_BRANCH;
    }
    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(eor)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(eor_inst));
    eor_inst* inst_cream = (eor_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldc)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldc_inst));
    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldm)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    if (BIT(inst, 15)) {
        inst_base->br = TransExtData::INDIRECT_BRANCH;
    }
    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(sxth)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(sxtb_inst));
    sxtb_inst* inst_cream = (sxtb_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->rotate = BITS(inst, 10, 11);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldr)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    if (BITS(inst, 12, 15) == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrcond)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    if (BITS(inst, 12, 15) == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(uxth)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(uxth_inst));
    uxth_inst* inst_cream = (uxth_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->rotate = BITS(inst, 10, 11);
    inst_cream->Rm = BITS(inst, 0, 3);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uxtah)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(uxtah_inst));
    uxtah_inst* inst_cream = (uxtah_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->rotate = BITS(inst, 10, 11);
    inst_cream->Rm = BITS(inst, 0, 3);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrb)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrbt)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOpLoadStoreT(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrd)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrex)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = (BITS(inst, 12, 15) == 15) ? TransExtData::INDIRECT_BRANCH
                                               : TransExtData::NON_BRANCH; // Branch if dest is R15

    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrexb)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(ldrex)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrexh)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(ldrex)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrexd)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(ldrex)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrh)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrsb)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrsh)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ldrt)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOpLoadStoreT(inst);

    if (BITS(inst, 12, 15) == 15) {
        inst_base->br = TransExtData::INDIRECT_BRANCH;
    }
    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(mcr)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(mcr_inst));
    mcr_inst* inst_cream = (mcr_inst*)inst_base->component;
    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->crn = BITS(inst, 16, 19);
    inst_cream->crm = BITS(inst, 0, 3);
    inst_cream->opcode_1 = BITS(inst, 21, 23);
    inst_cream->opcode_2 = BITS(inst, 5, 7);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->cp_num = BITS(inst, 8, 11);
    inst_cream->inst = inst;
    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(mcrr)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(mcrr_inst));
    mcrr_inst* const inst_cream = (mcrr_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->crm = BITS(inst, 0, 3);
    inst_cream->opcode_1 = BITS(inst, 4, 7);
    inst_cream->cp_num = BITS(inst, 8, 11);
    inst_cream->rt = BITS(inst, 12, 15);
    inst_cream->rt2 = BITS(inst, 16, 19);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(mla)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(mla_inst));
    mla_inst* inst_cream = (mla_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 12, 15);
    inst_cream->Rd = BITS(inst, 16, 19);
    inst_cream->Rs = BITS(inst, 8, 11);
    inst_cream->Rm = BITS(inst, 0, 3);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(mov)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(mov_inst));
    mov_inst* inst_cream = (mov_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15) {
        inst_base->br = TransExtData::INDIRECT_BRANCH;
    }
    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(mrc)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(mrc_inst));
    mrc_inst* inst_cream = (mrc_inst*)inst_base->component;
    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->crn = BITS(inst, 16, 19);
    inst_cream->crm = BITS(inst, 0, 3);
    inst_cream->opcode_1 = BITS(inst, 21, 23);
    inst_cream->opcode_2 = BITS(inst, 5, 7);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->cp_num = BITS(inst, 8, 11);
    inst_cream->inst = inst;
    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(mrrc)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(mcrr)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(mrs)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(mrs_inst));
    mrs_inst* inst_cream = (mrs_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->R = BIT(inst, 22);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(msr)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(msr_inst));
    msr_inst* inst_cream = (msr_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->field_mask = BITS(inst, 16, 19);
    inst_cream->R = BIT(inst, 22);
    inst_cream->inst = inst;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(mul)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(mul_inst));
    mul_inst* inst_cream = (mul_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->S = BIT(inst, 20);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rs = BITS(inst, 8, 11);
    inst_cream->Rd = BITS(inst, 16, 19);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(mvn)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(mvn_inst));
    mvn_inst* inst_cream = (mvn_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15) {
        inst_base->br = TransExtData::INDIRECT_BRANCH;
    }
    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(orr)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(orr_inst));
    orr_inst* inst_cream = (orr_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}

// NOP introduced in ARMv6K.
static ARM_INST_PTR INTERPRETER_TRANSLATE(nop)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst));

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(pkhbt)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(pkh_inst));
    pkh_inst* inst_cream = (pkh_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->imm = BITS(inst, 7, 11);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(pkhtb)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(pkhbt)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(pld)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(pld_inst));

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(qadd)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->op1 = BITS(inst, 21, 22);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(qdadd)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(qadd)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(qdsub)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(qadd)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(qsub)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(qadd)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(qadd8)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->op1 = BITS(inst, 20, 21);
    inst_cream->op2 = BITS(inst, 5, 7);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(qadd16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(qadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(qaddsubx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(qadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(qsub8)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(qadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(qsub16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(qadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(qsubaddx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(qadd8)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(rev)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(rev_inst));
    rev_inst* const inst_cream = (rev_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->op1 = BITS(inst, 20, 22);
    inst_cream->op2 = BITS(inst, 5, 7);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(rev16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(rev)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(revsh)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(rev)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(rfe)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* const inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = AL;
    inst_base->idx = index;
    inst_base->br = TransExtData::INDIRECT_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(rsb)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(rsb_inst));
    rsb_inst* inst_cream = (rsb_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(rsc)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(rsc_inst));
    rsc_inst* inst_cream = (rsc_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(sadd8)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->op1 = BITS(inst, 20, 21);
    inst_cream->op2 = BITS(inst, 5, 7);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(sadd16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(sadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(saddsubx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(sadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ssub8)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(sadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ssub16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(sadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ssubaddx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(sadd8)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(sbc)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(sbc_inst));
    sbc_inst* inst_cream = (sbc_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(sel)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->op1 = BITS(inst, 20, 22);
    inst_cream->op2 = BITS(inst, 5, 7);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(setend)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(setend_inst));
    setend_inst* const inst_cream = (setend_inst*)inst_base->component;

    inst_base->cond = AL;
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->set_bigend = BIT(inst, 9);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(sev)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst));

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(shadd8)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->op1 = BITS(inst, 20, 21);
    inst_cream->op2 = BITS(inst, 5, 7);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(shadd16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(shadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(shaddsubx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(shadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(shsub8)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(shadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(shsub16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(shadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(shsubaddx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(shadd8)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(smla)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(smla_inst));
    smla_inst* inst_cream = (smla_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->x = BIT(inst, 5);
    inst_cream->y = BIT(inst, 6);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rs = BITS(inst, 8, 11);
    inst_cream->Rd = BITS(inst, 16, 19);
    inst_cream->Rn = BITS(inst, 12, 15);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(smlad)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(smlad_inst));
    smlad_inst* const inst_cream = (smlad_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->m = BIT(inst, 5);
    inst_cream->Rn = BITS(inst, 0, 3);
    inst_cream->Rm = BITS(inst, 8, 11);
    inst_cream->Rd = BITS(inst, 16, 19);
    inst_cream->Ra = BITS(inst, 12, 15);
    inst_cream->op1 = BITS(inst, 20, 22);
    inst_cream->op2 = BITS(inst, 5, 7);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(smuad)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(smlad)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(smusd)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(smlad)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(smlsd)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(smlad)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(smlal)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(umlal_inst));
    umlal_inst* inst_cream = (umlal_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->S = BIT(inst, 20);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rs = BITS(inst, 8, 11);
    inst_cream->RdHi = BITS(inst, 16, 19);
    inst_cream->RdLo = BITS(inst, 12, 15);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(smlalxy)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(smlalxy_inst));
    smlalxy_inst* const inst_cream = (smlalxy_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->x = BIT(inst, 5);
    inst_cream->y = BIT(inst, 6);
    inst_cream->RdLo = BITS(inst, 12, 15);
    inst_cream->RdHi = BITS(inst, 16, 19);
    inst_cream->Rn = BITS(inst, 0, 4);
    inst_cream->Rm = BITS(inst, 8, 11);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(smlaw)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(smlad_inst));
    smlad_inst* const inst_cream = (smlad_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Ra = BITS(inst, 12, 15);
    inst_cream->Rm = BITS(inst, 8, 11);
    inst_cream->Rn = BITS(inst, 0, 3);
    inst_cream->Rd = BITS(inst, 16, 19);
    inst_cream->m = BIT(inst, 6);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(smlald)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(smlald_inst));
    smlald_inst* const inst_cream = (smlald_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rm = BITS(inst, 8, 11);
    inst_cream->Rn = BITS(inst, 0, 3);
    inst_cream->RdLo = BITS(inst, 12, 15);
    inst_cream->RdHi = BITS(inst, 16, 19);
    inst_cream->swap = BIT(inst, 5);
    inst_cream->op1 = BITS(inst, 20, 22);
    inst_cream->op2 = BITS(inst, 5, 7);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(smlsld)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(smlald)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(smmla)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(smlad_inst));
    smlad_inst* const inst_cream = (smlad_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->m = BIT(inst, 5);
    inst_cream->Ra = BITS(inst, 12, 15);
    inst_cream->Rm = BITS(inst, 8, 11);
    inst_cream->Rn = BITS(inst, 0, 3);
    inst_cream->Rd = BITS(inst, 16, 19);
    inst_cream->op1 = BITS(inst, 20, 22);
    inst_cream->op2 = BITS(inst, 5, 7);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(smmls)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(smmla)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(smmul)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(smmla)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(smul)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(smul_inst));
    smul_inst* inst_cream = (smul_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rd = BITS(inst, 16, 19);
    inst_cream->Rs = BITS(inst, 8, 11);
    inst_cream->Rm = BITS(inst, 0, 3);

    inst_cream->x = BIT(inst, 5);
    inst_cream->y = BIT(inst, 6);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(smull)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(umull_inst));
    umull_inst* inst_cream = (umull_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->S = BIT(inst, 20);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rs = BITS(inst, 8, 11);
    inst_cream->RdHi = BITS(inst, 16, 19);
    inst_cream->RdLo = BITS(inst, 12, 15);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(smulw)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(smlad_inst));
    smlad_inst* inst_cream = (smlad_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->m = BIT(inst, 6);
    inst_cream->Rm = BITS(inst, 8, 11);
    inst_cream->Rn = BITS(inst, 0, 3);
    inst_cream->Rd = BITS(inst, 16, 19);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(srs)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* const inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = AL;
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(ssat)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ssat_inst));
    ssat_inst* const inst_cream = (ssat_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rn = BITS(inst, 0, 3);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->imm5 = BITS(inst, 7, 11);
    inst_cream->sat_imm = BITS(inst, 16, 20);
    inst_cream->shift_type = BIT(inst, 6);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(ssat16)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ssat_inst));
    ssat_inst* const inst_cream = (ssat_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rn = BITS(inst, 0, 3);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->sat_imm = BITS(inst, 16, 19);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(stc)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(stc_inst));
    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(stm)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);
    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(sxtb)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(sxtb_inst));
    sxtb_inst* inst_cream = (sxtb_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->rotate = BITS(inst, 10, 11);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(str)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uxtb)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(uxth_inst));
    uxth_inst* inst_cream = (uxth_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->rotate = BITS(inst, 10, 11);
    inst_cream->Rm = BITS(inst, 0, 3);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uxtab)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(uxtab_inst));
    uxtab_inst* inst_cream = (uxtab_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->rotate = BITS(inst, 10, 11);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(strb)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(strbt)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOpLoadStoreT(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(strd)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(strex)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->Rm = BITS(inst, 0, 3);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(strexb)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(strex)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(strexh)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(strex)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(strexd)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(strex)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(strh)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(strt)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(ldst_inst));
    ldst_inst* inst_cream = (ldst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->inst = inst;
    inst_cream->get_addr = GetAddressingOpLoadStoreT(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(sub)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(sub_inst));
    sub_inst* inst_cream = (sub_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    if (inst_cream->Rd == 15)
        inst_base->br = TransExtData::INDIRECT_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(swi)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(swi_inst));
    swi_inst* inst_cream = (swi_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->num = BITS(inst, 0, 23);
    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(swp)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(swp_inst));
    swp_inst* inst_cream = (swp_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->Rm = BITS(inst, 0, 3);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(swpb)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(swp_inst));
    swp_inst* inst_cream = (swp_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->Rm = BITS(inst, 0, 3);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(sxtab)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(sxtab_inst));
    sxtab_inst* inst_cream = (sxtab_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->rotate = BITS(inst, 10, 11);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(sxtab16)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(sxtab_inst));
    sxtab_inst* const inst_cream = (sxtab_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->rotate = BITS(inst, 10, 11);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(sxtb16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(sxtab16)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(sxtah)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(sxtah_inst));
    sxtah_inst* inst_cream = (sxtah_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->rotate = BITS(inst, 10, 11);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(teq)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(teq_inst));
    teq_inst* inst_cream = (teq_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(tst)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(tst_inst));
    tst_inst* inst_cream = (tst_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->I = BIT(inst, 25);
    inst_cream->S = BIT(inst, 20);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->shifter_operand = BITS(inst, 0, 11);
    inst_cream->shtop_func = GetShifterOp(inst);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(uadd8)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->op1 = BITS(inst, 20, 21);
    inst_cream->op2 = BITS(inst, 5, 7);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uadd16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uaddsubx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(usub8)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(usub16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(usubaddx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uadd8)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(uhadd8)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->op1 = BITS(inst, 20, 21);
    inst_cream->op2 = BITS(inst, 5, 7);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uhadd16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uhadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uhaddsubx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uhadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uhsub8)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uhadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uhsub16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uhadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uhsubaddx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uhadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(umaal)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(umaal_inst));
    umaal_inst* const inst_cream = (umaal_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rm = BITS(inst, 8, 11);
    inst_cream->Rn = BITS(inst, 0, 3);
    inst_cream->RdLo = BITS(inst, 12, 15);
    inst_cream->RdHi = BITS(inst, 16, 19);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(umlal)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(umlal_inst));
    umlal_inst* inst_cream = (umlal_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->S = BIT(inst, 20);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rs = BITS(inst, 8, 11);
    inst_cream->RdHi = BITS(inst, 16, 19);
    inst_cream->RdLo = BITS(inst, 12, 15);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(umull)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(umull_inst));
    umull_inst* inst_cream = (umull_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->S = BIT(inst, 20);
    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rs = BITS(inst, 8, 11);
    inst_cream->RdHi = BITS(inst, 16, 19);
    inst_cream->RdLo = BITS(inst, 12, 15);

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(b_2_thumb)(ARMul_State *state, unsigned int tinst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(b_2_thumb));
    b_2_thumb* inst_cream = (b_2_thumb*)inst_base->component;

    inst_cream->imm = ((tinst & 0x3FF) << 1) | ((tinst & (1 << 10)) ? 0xFFFFF800 : 0);

    inst_base->idx = index;
    inst_base->br = TransExtData::DIRECT_BRANCH;

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(b_cond_thumb)(ARMul_State *state, unsigned int tinst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(b_cond_thumb));
    b_cond_thumb* inst_cream = (b_cond_thumb*)inst_base->component;

    inst_cream->imm = (((tinst & 0x7F) << 1) | ((tinst & (1 << 7)) ? 0xFFFFFF00 : 0));
    inst_cream->cond = ((tinst >> 8) & 0xf);
    inst_base->idx = index;
    inst_base->br = TransExtData::DIRECT_BRANCH;

    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(bl_1_thumb)(ARMul_State *state, unsigned int tinst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(bl_1_thumb));
    bl_1_thumb* inst_cream = (bl_1_thumb*)inst_base->component;

    inst_cream->imm = (((tinst & 0x07FF) << 12) | ((tinst & (1 << 10)) ? 0xFF800000 : 0));

    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;
    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(bl_2_thumb)(ARMul_State *state, unsigned int tinst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(bl_2_thumb));
    bl_2_thumb* inst_cream = (bl_2_thumb*)inst_base->component;

    inst_cream->imm = (tinst & 0x07FF) << 1;

    inst_base->idx = index;
    inst_base->br = TransExtData::DIRECT_BRANCH;
    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(blx_1_thumb)(ARMul_State *state, unsigned int tinst, int index) {
    arm_inst* inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(blx_1_thumb));
    blx_1_thumb* inst_cream = (blx_1_thumb*)inst_base->component;

    inst_cream->imm = (tinst & 0x07FF) << 1;
    inst_cream->instr = tinst;

    inst_base->idx = index;
    inst_base->br = TransExtData::DIRECT_BRANCH;
    return inst_base;
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(uqadd8)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->op1 = BITS(inst, 20, 21);
    inst_cream->op2 = BITS(inst, 5, 7);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uqadd16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uqadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uqaddsubx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uqadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uqsub8)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uqadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uqsub16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uqadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uqsubaddx)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uqadd8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(usada8)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(generic_arm_inst));
    generic_arm_inst* const inst_cream = (generic_arm_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->op1 = BITS(inst, 20, 24);
    inst_cream->op2 = BITS(inst, 5, 7);
    inst_cream->Rd = BITS(inst, 16, 19);
    inst_cream->Rm = BITS(inst, 8, 11);
    inst_cream->Rn = BITS(inst, 0, 3);
    inst_cream->Ra = BITS(inst, 12, 15);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(usad8)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(usada8)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(usat)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(ssat)(state, inst, index);
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(usat16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(ssat16)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(uxtab16)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst) + sizeof(uxtab_inst));
    uxtab_inst* const inst_cream = (uxtab_inst*)inst_base->component;

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    inst_cream->Rm = BITS(inst, 0, 3);
    inst_cream->Rn = BITS(inst, 16, 19);
    inst_cream->Rd = BITS(inst, 12, 15);
    inst_cream->rotate = BITS(inst, 10, 11);

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(uxtb16)(ARMul_State *state, unsigned int inst, int index) {
    return INTERPRETER_TRANSLATE(uxtab16)(state, inst, index);
}

static ARM_INST_PTR INTERPRETER_TRANSLATE(wfe)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst));

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(wfi)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst));

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    return inst_base;
}
static ARM_INST_PTR INTERPRETER_TRANSLATE(yield)(ARMul_State *state, unsigned int inst, int index) {
    arm_inst* const inst_base = (arm_inst*)AllocBuffer(state, sizeof(arm_inst));

    inst_base->cond = BITS(inst, 28, 31);
    inst_base->idx = index;
    inst_base->br = TransExtData::NON_BRANCH;

    return inst_base;
}

// Floating point VFPv3 instructions
#define VFP_INTERPRETER_TRANS
#include <cpu/dyncom/vfp/vfpinstr.h>
#undef VFP_INTERPRETER_TRANS

const transop_fp_t arm_instruction_trans[] = {
    INTERPRETER_TRANSLATE(vmla),
    INTERPRETER_TRANSLATE(vmls),
    INTERPRETER_TRANSLATE(vnmla),
    INTERPRETER_TRANSLATE(vnmls),
    INTERPRETER_TRANSLATE(vnmul),
    INTERPRETER_TRANSLATE(vmul),
    INTERPRETER_TRANSLATE(vadd),
    INTERPRETER_TRANSLATE(vsub),
    INTERPRETER_TRANSLATE(vdiv),
    INTERPRETER_TRANSLATE(vmovi),
    INTERPRETER_TRANSLATE(vmovr),
    INTERPRETER_TRANSLATE(vabs),
    INTERPRETER_TRANSLATE(vneg),
    INTERPRETER_TRANSLATE(vsqrt),
    INTERPRETER_TRANSLATE(vcmp),
    INTERPRETER_TRANSLATE(vcmp2),
    INTERPRETER_TRANSLATE(vcvtbds),
    INTERPRETER_TRANSLATE(vcvtbff),
    INTERPRETER_TRANSLATE(vcvtbfi),
    INTERPRETER_TRANSLATE(vmovbrs),
    INTERPRETER_TRANSLATE(vmsr),
    INTERPRETER_TRANSLATE(vmovbrc),
    INTERPRETER_TRANSLATE(vmrs),
    INTERPRETER_TRANSLATE(vmovbcr),
    INTERPRETER_TRANSLATE(vmovbrrss),
    INTERPRETER_TRANSLATE(vmovbrrd),
    INTERPRETER_TRANSLATE(vstr),
    INTERPRETER_TRANSLATE(vpush),
    INTERPRETER_TRANSLATE(vstm),
    INTERPRETER_TRANSLATE(vpop),
    INTERPRETER_TRANSLATE(vldr),
    INTERPRETER_TRANSLATE(vldm),

    INTERPRETER_TRANSLATE(srs),
    INTERPRETER_TRANSLATE(rfe),
    INTERPRETER_TRANSLATE(bkpt),
    INTERPRETER_TRANSLATE(blx),
    INTERPRETER_TRANSLATE(cps),
    INTERPRETER_TRANSLATE(pld),
    INTERPRETER_TRANSLATE(setend),
    INTERPRETER_TRANSLATE(clrex),
    INTERPRETER_TRANSLATE(rev16),
    INTERPRETER_TRANSLATE(usad8),
    INTERPRETER_TRANSLATE(sxtb),
    INTERPRETER_TRANSLATE(uxtb),
    INTERPRETER_TRANSLATE(sxth),
    INTERPRETER_TRANSLATE(sxtb16),
    INTERPRETER_TRANSLATE(uxth),
    INTERPRETER_TRANSLATE(uxtb16),
    INTERPRETER_TRANSLATE(cpy),
    INTERPRETER_TRANSLATE(uxtab),
    INTERPRETER_TRANSLATE(ssub8),
    INTERPRETER_TRANSLATE(shsub8),
    INTERPRETER_TRANSLATE(ssubaddx),
    INTERPRETER_TRANSLATE(strex),
    INTERPRETER_TRANSLATE(strexb),
    INTERPRETER_TRANSLATE(swp),
    INTERPRETER_TRANSLATE(swpb),
    INTERPRETER_TRANSLATE(ssub16),
    INTERPRETER_TRANSLATE(ssat16),
    INTERPRETER_TRANSLATE(shsubaddx),
    INTERPRETER_TRANSLATE(qsubaddx),
    INTERPRETER_TRANSLATE(shaddsubx),
    INTERPRETER_TRANSLATE(shadd8),
    INTERPRETER_TRANSLATE(shadd16),
    INTERPRETER_TRANSLATE(sel),
    INTERPRETER_TRANSLATE(saddsubx),
    INTERPRETER_TRANSLATE(sadd8),
    INTERPRETER_TRANSLATE(sadd16),
    INTERPRETER_TRANSLATE(shsub16),
    INTERPRETER_TRANSLATE(umaal),
    INTERPRETER_TRANSLATE(uxtab16),
    INTERPRETER_TRANSLATE(usubaddx),
    INTERPRETER_TRANSLATE(usub8),
    INTERPRETER_TRANSLATE(usub16),
    INTERPRETER_TRANSLATE(usat16),
    INTERPRETER_TRANSLATE(usada8),
    INTERPRETER_TRANSLATE(uqsubaddx),
    INTERPRETER_TRANSLATE(uqsub8),
    INTERPRETER_TRANSLATE(uqsub16),
    INTERPRETER_TRANSLATE(uqaddsubx),
    INTERPRETER_TRANSLATE(uqadd8),
    INTERPRETER_TRANSLATE(uqadd16),
    INTERPRETER_TRANSLATE(sxtab),
    INTERPRETER_TRANSLATE(uhsubaddx),
    INTERPRETER_TRANSLATE(uhsub8),
    INTERPRETER_TRANSLATE(uhsub16),
    INTERPRETER_TRANSLATE(uhaddsubx),
    INTERPRETER_TRANSLATE(uhadd8),
    INTERPRETER_TRANSLATE(uhadd16),
    INTERPRETER_TRANSLATE(uaddsubx),
    INTERPRETER_TRANSLATE(uadd8),
    INTERPRETER_TRANSLATE(uadd16),
    INTERPRETER_TRANSLATE(sxtah),
    INTERPRETER_TRANSLATE(sxtab16),
    INTERPRETER_TRANSLATE(qadd8),
    INTERPRETER_TRANSLATE(bxj),
    INTERPRETER_TRANSLATE(clz),
    INTERPRETER_TRANSLATE(uxtah),
    INTERPRETER_TRANSLATE(bx),
    INTERPRETER_TRANSLATE(rev),
    INTERPRETER_TRANSLATE(blx),
    INTERPRETER_TRANSLATE(revsh),
    INTERPRETER_TRANSLATE(qadd),
    INTERPRETER_TRANSLATE(qadd16),
    INTERPRETER_TRANSLATE(qaddsubx),
    INTERPRETER_TRANSLATE(ldrex),
    INTERPRETER_TRANSLATE(qdadd),
    INTERPRETER_TRANSLATE(qdsub),
    INTERPRETER_TRANSLATE(qsub),
    INTERPRETER_TRANSLATE(ldrexb),
    INTERPRETER_TRANSLATE(qsub8),
    INTERPRETER_TRANSLATE(qsub16),
    INTERPRETER_TRANSLATE(smuad),
    INTERPRETER_TRANSLATE(smmul),
    INTERPRETER_TRANSLATE(smusd),
    INTERPRETER_TRANSLATE(smlsd),
    INTERPRETER_TRANSLATE(smlsld),
    INTERPRETER_TRANSLATE(smmla),
    INTERPRETER_TRANSLATE(smmls),
    INTERPRETER_TRANSLATE(smlald),
    INTERPRETER_TRANSLATE(smlad),
    INTERPRETER_TRANSLATE(smlaw),
    INTERPRETER_TRANSLATE(smulw),
    INTERPRETER_TRANSLATE(pkhtb),
    INTERPRETER_TRANSLATE(pkhbt),
    INTERPRETER_TRANSLATE(smul),
    INTERPRETER_TRANSLATE(smlalxy),
    INTERPRETER_TRANSLATE(smla),
    INTERPRETER_TRANSLATE(mcrr),
    INTERPRETER_TRANSLATE(mrrc),
    INTERPRETER_TRANSLATE(cmp),
    INTERPRETER_TRANSLATE(tst),
    INTERPRETER_TRANSLATE(teq),
    INTERPRETER_TRANSLATE(cmn),
    INTERPRETER_TRANSLATE(smull),
    INTERPRETER_TRANSLATE(umull),
    INTERPRETER_TRANSLATE(umlal),
    INTERPRETER_TRANSLATE(smlal),
    INTERPRETER_TRANSLATE(mul),
    INTERPRETER_TRANSLATE(mla),
    INTERPRETER_TRANSLATE(ssat),
    INTERPRETER_TRANSLATE(usat),
    INTERPRETER_TRANSLATE(mrs),
    INTERPRETER_TRANSLATE(msr),
    INTERPRETER_TRANSLATE(and),
    INTERPRETER_TRANSLATE(bic),
    INTERPRETER_TRANSLATE(ldm),
    INTERPRETER_TRANSLATE(eor),
    INTERPRETER_TRANSLATE(add),
    INTERPRETER_TRANSLATE(rsb),
    INTERPRETER_TRANSLATE(rsc),
    INTERPRETER_TRANSLATE(sbc),
    INTERPRETER_TRANSLATE(adc),
    INTERPRETER_TRANSLATE(sub),
    INTERPRETER_TRANSLATE(orr),
    INTERPRETER_TRANSLATE(mvn),
    INTERPRETER_TRANSLATE(mov),
    INTERPRETER_TRANSLATE(stm),
    INTERPRETER_TRANSLATE(ldm),
    INTERPRETER_TRANSLATE(ldrsh),
    INTERPRETER_TRANSLATE(stm),
    INTERPRETER_TRANSLATE(ldm),
    INTERPRETER_TRANSLATE(ldrsb),
    INTERPRETER_TRANSLATE(strd),
    INTERPRETER_TRANSLATE(ldrh),
    INTERPRETER_TRANSLATE(strh),
    INTERPRETER_TRANSLATE(ldrd),
    INTERPRETER_TRANSLATE(strt),
    INTERPRETER_TRANSLATE(strbt),
    INTERPRETER_TRANSLATE(ldrbt),
    INTERPRETER_TRANSLATE(ldrt),
    INTERPRETER_TRANSLATE(mrc),
    INTERPRETER_TRANSLATE(mcr),
    INTERPRETER_TRANSLATE(msr),
    INTERPRETER_TRANSLATE(msr),
    INTERPRETER_TRANSLATE(msr),
    INTERPRETER_TRANSLATE(msr),
    INTERPRETER_TRANSLATE(msr),
    INTERPRETER_TRANSLATE(ldrb),
    INTERPRETER_TRANSLATE(strb),
    INTERPRETER_TRANSLATE(ldr),
    INTERPRETER_TRANSLATE(ldrcond),
    INTERPRETER_TRANSLATE(str),
    INTERPRETER_TRANSLATE(cdp),
    INTERPRETER_TRANSLATE(stc),
    INTERPRETER_TRANSLATE(ldc),
    INTERPRETER_TRANSLATE(ldrexd),
    INTERPRETER_TRANSLATE(strexd),
    INTERPRETER_TRANSLATE(ldrexh),
    INTERPRETER_TRANSLATE(strexh),
    INTERPRETER_TRANSLATE(nop),
    INTERPRETER_TRANSLATE(yield),
    INTERPRETER_TRANSLATE(wfe),
    INTERPRETER_TRANSLATE(wfi),
    INTERPRETER_TRANSLATE(sev),
    INTERPRETER_TRANSLATE(swi),
    INTERPRETER_TRANSLATE(bbl),

    // All the thumb instructions should be placed the end of table
    INTERPRETER_TRANSLATE(b_2_thumb),
    INTERPRETER_TRANSLATE(b_cond_thumb),
    INTERPRETER_TRANSLATE(bl_1_thumb),
    INTERPRETER_TRANSLATE(bl_2_thumb),
    INTERPRETER_TRANSLATE(blx_1_thumb),
};

const std::size_t arm_instruction_trans_len = sizeof(arm_instruction_trans) / sizeof(transop_fp_t);
