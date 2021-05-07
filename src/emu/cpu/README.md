## Contents

This folder contains code with can instantiate multiple CPU emulator kinds.

- Supports: Dynarmic, R12L1 and Dyncom.
- Removed: Unicorn **(But thanks a lot!)**

## Notice about some backends

### Dyncom

Dyncom is a CPU interperter modified from SkyEye interpreter and with added ARMv6K support from Citra team. It is brought to here for debugging purposes. The license of all files still remains from Citra team, under **GPLv2**.

The version here contains some fixes:
- ADD/SUB PC should not use aligned by word address. Address should be kept sane.
- ADR instructions that got translated to ADD/SUB are now detected, and the execute process will aligned current PC by word unit.
- System calls should also reload NZCVT flags.

### R12L1

**R**ecompiler for **12L1** (EKA2L1), is an ARM to ARM translator, with only 32-bit guest support intended.

Please note that the emulator took the decoder design and revised the validation of instructions a lot from **Dynarmic**. License will be added consequencely, or immediately under strict requests.

The emulator does mostly direct instruction translation, with managing register allocation only for GPR registers and directly use the VFP/NEON register banks with read/write tracking.