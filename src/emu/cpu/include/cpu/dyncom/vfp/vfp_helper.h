/*
    vfp/vfp.h - ARM VFPv3 emulation unit - SoftFloat lib helper
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

/*
 *  The following code is derivative from Linux Android kernel vfp
 *  floating point support.
 *
 *  Copyright (C) 2004 ARM Limited.
 *  Written by Deep Blue Solutions Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#include <cstdio>
#include <common/types.h>
#include <cpu/dyncom/vfp/asm_vfp.h>
#include <cpu/dyncom/armstate.h>

#define do_div(n, base)                                                                            \
    { n /= base; }

enum : std::uint32_t {
    FOP_MASK = 0x00b00040,
    FOP_FMAC = 0x00000000,
    FOP_FNMAC = 0x00000040,
    FOP_FMSC = 0x00100000,
    FOP_FNMSC = 0x00100040,
    FOP_FMUL = 0x00200000,
    FOP_FNMUL = 0x00200040,
    FOP_FADD = 0x00300000,
    FOP_FSUB = 0x00300040,
    FOP_FDIV = 0x00800000,
    FOP_EXT = 0x00b00040
};

#define FOP_TO_IDX(inst) ((inst & 0x00b00000) >> 20 | (inst & (1 << 6)) >> 4)

enum : std::uint32_t {
    FEXT_MASK = 0x000f0080,
    FEXT_FCPY = 0x00000000,
    FEXT_FABS = 0x00000080,
    FEXT_FNEG = 0x00010000,
    FEXT_FSQRT = 0x00010080,
    FEXT_FCMP = 0x00040000,
    FEXT_FCMPE = 0x00040080,
    FEXT_FCMPZ = 0x00050000,
    FEXT_FCMPEZ = 0x00050080,
    FEXT_FCVT = 0x00070080,
    FEXT_FUITO = 0x00080000,
    FEXT_FSITO = 0x00080080,
    FEXT_FTOUI = 0x000c0000,
    FEXT_FTOUIZ = 0x000c0080,
    FEXT_FTOSI = 0x000d0000,
    FEXT_FTOSIZ = 0x000d0080
};

#define FEXT_TO_IDX(inst) ((inst & 0x000f0000) >> 15 | (inst & (1 << 7)) >> 7)

#define vfp_get_sd(inst) ((inst & 0x0000f000) >> 11 | (inst & (1 << 22)) >> 22)
#define vfp_get_dd(inst) ((inst & 0x0000f000) >> 12 | (inst & (1 << 22)) >> 18)
#define vfp_get_sm(inst) ((inst & 0x0000000f) << 1 | (inst & (1 << 5)) >> 5)
#define vfp_get_dm(inst) ((inst & 0x0000000f) | (inst & (1 << 5)) >> 1)
#define vfp_get_sn(inst) ((inst & 0x000f0000) >> 15 | (inst & (1 << 7)) >> 7)
#define vfp_get_dn(inst) ((inst & 0x000f0000) >> 16 | (inst & (1 << 7)) >> 3)

#define vfp_single(inst) (((inst)&0x0000f00) == 0xa00)

inline std::uint32_t vfp_shiftright32jamming(std::uint32_t val, unsigned int shift) {
    if (shift) {
        if (shift < 32)
            val = val >> shift | ((val << (32 - shift)) != 0);
        else
            val = val != 0;
    }
    return val;
}

inline std::uint64_t  vfp_shiftright64jamming(std::uint64_t  val, unsigned int shift) {
    if (shift) {
        if (shift < 64)
            val = val >> shift | ((val << (64 - shift)) != 0);
        else
            val = val != 0;
    }
    return val;
}

inline std::uint32_t vfp_hi64to32jamming(std::uint64_t  val) {
    std::uint32_t v;
    std::uint32_t highval = val >> 32;
    std::uint32_t lowval = val & 0xffffffff;

    if (lowval >= 1)
        v = highval | 1;
    else
        v = highval;

    return v;
}

inline void add128(std::uint64_t *resh, std::uint64_t *resl, std::uint64_t  nh, std::uint64_t  nl, std::uint64_t  mh, std::uint64_t  ml) {
    *resl = nl + ml;
    *resh = nh + mh;
    if (*resl < nl)
        *resh += 1;
}

inline void sub128(std::uint64_t *resh, std::uint64_t *resl, std::uint64_t  nh, std::uint64_t  nl, std::uint64_t  mh, std::uint64_t  ml) {
    *resl = nl - ml;
    *resh = nh - mh;
    if (*resl > nl)
        *resh -= 1;
}

inline void mul64to128(std::uint64_t *resh, std::uint64_t *resl, std::uint64_t  n, std::uint64_t  m) {
    std::uint32_t nh, nl, mh, ml;
    std::uint64_t  rh, rma, rmb, rl;

    nl = static_cast<std::uint32_t>(n);
    ml = static_cast<std::uint32_t>(m);
    rl = (std::uint64_t)nl * ml;

    nh = n >> 32;
    rma = (std::uint64_t)nh * ml;

    mh = m >> 32;
    rmb = (std::uint64_t)nl * mh;
    rma += rmb;

    rh = (std::uint64_t)nh * mh;
    rh += ((std::uint64_t)(rma < rmb) << 32) + (rma >> 32);

    rma <<= 32;
    rl += rma;
    rh += (rl < rma);

    *resl = rl;
    *resh = rh;
}

inline void shift64left(std::uint64_t *resh, std::uint64_t *resl, std::uint64_t  n) {
    *resh = n >> 63;
    *resl = n << 1;
}

inline std::uint64_t  vfp_hi64multiply64(std::uint64_t  n, std::uint64_t  m) {
    std::uint64_t  rh, rl;
    mul64to128(&rh, &rl, n, m);
    return rh | (rl != 0);
}

inline std::uint64_t  vfp_estimate_div128to64(std::uint64_t  nh, std::uint64_t  nl, std::uint64_t  m) {
    std::uint64_t  mh, ml, remh, reml, termh, terml, z;

    if (nh >= m)
        return ~0ULL;
    mh = m >> 32;
    if (mh << 32 <= nh) {
        z = 0xffffffff00000000ULL;
    } else {
        z = nh;
        do_div(z, mh);
        z <<= 32;
    }
    mul64to128(&termh, &terml, m, z);
    sub128(&remh, &reml, nh, nl, termh, terml);
    ml = m << 32;
    while ((std::int64_t)remh < 0) {
        z -= 0x100000000ULL;
        add128(&remh, &reml, remh, reml, mh, ml);
    }
    remh = (remh << 32) | (reml >> 32);
    if (mh << 32 <= remh) {
        z |= 0xffffffff;
    } else {
        do_div(remh, mh);
        z |= remh;
    }
    return z;
}

// Operations on unpacked elements
#define vfp_sign_negate(sign) (sign ^ 0x8000)

// Single-precision
struct vfp_single {
    std::int16_t  exponent;
    std::uint16_t  sign;
    std::uint32_t significand;
};

// VFP_SINGLE_MANTISSA_BITS - number of bits in the mantissa
// VFP_SINGLE_EXPONENT_BITS - number of bits in the exponent
// VFP_SINGLE_LOW_BITS - number of low bits in the unpacked significand
// which are not propagated to the float upon packing.
#define VFP_SINGLE_MANTISSA_BITS (23)
#define VFP_SINGLE_EXPONENT_BITS (8)
#define VFP_SINGLE_LOW_BITS (32 - VFP_SINGLE_MANTISSA_BITS - 2)
#define VFP_SINGLE_LOW_BITS_MASK ((1 << VFP_SINGLE_LOW_BITS) - 1)

// The bit in an unpacked float which indicates that it is a quiet NaN
#define VFP_SINGLE_SIGNIFICAND_QNAN (1 << (VFP_SINGLE_MANTISSA_BITS - 1 + VFP_SINGLE_LOW_BITS))

// Operations on packed single-precision numbers
#define vfp_single_packed_sign(v) ((v)&0x80000000)
#define vfp_single_packed_negate(v) ((v) ^ 0x80000000)
#define vfp_single_packed_abs(v) ((v) & ~0x80000000)
#define vfp_single_packed_exponent(v)                                                              \
    (((v) >> VFP_SINGLE_MANTISSA_BITS) & ((1 << VFP_SINGLE_EXPONENT_BITS) - 1))
#define vfp_single_packed_mantissa(v) ((v) & ((1 << VFP_SINGLE_MANTISSA_BITS) - 1))

enum : std::uint32_t {
    VFP_NUMBER = (1 << 0),
    VFP_ZERO = (1 << 1),
    VFP_DENORMAL = (1 << 2),
    VFP_INFINITY = (1 << 3),
    VFP_NAN = (1 << 4),
    VFP_NAN_SIGNAL = (1 << 5),

    VFP_QNAN = (VFP_NAN),
    VFP_SNAN = (VFP_NAN | VFP_NAN_SIGNAL)
};

inline int vfp_single_type(const vfp_single* s) {
    int type = VFP_NUMBER;
    if (s->exponent == 255) {
        if (s->significand == 0)
            type = VFP_INFINITY;
        else if (s->significand & VFP_SINGLE_SIGNIFICAND_QNAN)
            type = VFP_QNAN;
        else
            type = VFP_SNAN;
    } else if (s->exponent == 0) {
        if (s->significand == 0)
            type |= VFP_ZERO;
        else
            type |= VFP_DENORMAL;
    }
    return type;
}

// Unpack a single-precision float.  Note that this returns the magnitude
// of the single-precision float mantissa with the 1. if necessary,
// aligned to bit 30.
inline std::uint32_t vfp_single_unpack(vfp_single* s, std::int32_t  val, std::uint32_t fpscr) {
    std::uint32_t exceptions = 0;
    s->sign = vfp_single_packed_sign(val) >> 16;
    s->exponent = vfp_single_packed_exponent(val);

    std::uint32_t significand = ((std::uint32_t)val << (32 - VFP_SINGLE_MANTISSA_BITS)) >> 2;
    if (s->exponent && s->exponent != 255)
        significand |= 0x40000000;
    s->significand = significand;

    // If flush-to-zero mode is enabled, turn the denormal into zero.
    // On a VFPv2 architecture, the sign of the zero is always positive.
    if ((fpscr & FPSCR_FLUSH_TO_ZERO) != 0 && (vfp_single_type(s) & VFP_DENORMAL) != 0) {
        s->sign = 0;
        s->exponent = 0;
        s->significand = 0;
        exceptions |= FPSCR_IDC;
    }
    return exceptions;
}

// Re-pack a single-precision float. This assumes that the float is
// already normalised such that the MSB is bit 30, _not_ bit 31.
inline std::int32_t  vfp_single_pack(const vfp_single* s) {
    std::uint32_t val = (s->sign << 16) + (s->exponent << VFP_SINGLE_MANTISSA_BITS) +
              (s->significand >> VFP_SINGLE_LOW_BITS);
    return (std::int32_t)val;
}

std::uint32_t vfp_single_normaliseround(ARMul_State* state, int sd, vfp_single* vs, std::uint32_t fpscr, std::uint32_t exceptions,
                              const char* func);

// Double-precision
struct vfp_double {
    std::int16_t  exponent;
    std::uint16_t  sign;
    std::uint64_t  significand;
};

#define VFP_DOUBLE_MANTISSA_BITS (52)
#define VFP_DOUBLE_EXPONENT_BITS (11)
#define VFP_DOUBLE_LOW_BITS (64 - VFP_DOUBLE_MANTISSA_BITS - 2)
#define VFP_DOUBLE_LOW_BITS_MASK ((1 << VFP_DOUBLE_LOW_BITS) - 1)

// The bit in an unpacked double which indicates that it is a quiet NaN
#define VFP_DOUBLE_SIGNIFICAND_QNAN (1ULL << (VFP_DOUBLE_MANTISSA_BITS - 1 + VFP_DOUBLE_LOW_BITS))

// Operations on packed single-precision numbers
#define vfp_double_packed_sign(v) ((v) & (1ULL << 63))
#define vfp_double_packed_negate(v) ((v) ^ (1ULL << 63))
#define vfp_double_packed_abs(v) ((v) & ~(1ULL << 63))
#define vfp_double_packed_exponent(v)                                                              \
    (((v) >> VFP_DOUBLE_MANTISSA_BITS) & ((1 << VFP_DOUBLE_EXPONENT_BITS) - 1))
#define vfp_double_packed_mantissa(v) ((v) & ((1ULL << VFP_DOUBLE_MANTISSA_BITS) - 1))

inline int vfp_double_type(const vfp_double* s) {
    int type = VFP_NUMBER;
    if (s->exponent == 2047) {
        if (s->significand == 0)
            type = VFP_INFINITY;
        else if (s->significand & VFP_DOUBLE_SIGNIFICAND_QNAN)
            type = VFP_QNAN;
        else
            type = VFP_SNAN;
    } else if (s->exponent == 0) {
        if (s->significand == 0)
            type |= VFP_ZERO;
        else
            type |= VFP_DENORMAL;
    }
    return type;
}

// Unpack a double-precision float.  Note that this returns the magnitude
// of the double-precision float mantissa with the 1. if necessary,
// aligned to bit 62.
inline std::uint32_t vfp_double_unpack(vfp_double* s, std::int64_t val, std::uint32_t fpscr) {
    std::uint32_t exceptions = 0;
    s->sign = vfp_double_packed_sign(val) >> 48;
    s->exponent = vfp_double_packed_exponent(val);

    std::uint64_t  significand = ((std::uint64_t)val << (64 - VFP_DOUBLE_MANTISSA_BITS)) >> 2;
    if (s->exponent && s->exponent != 2047)
        significand |= (1ULL << 62);
    s->significand = significand;

    // If flush-to-zero mode is enabled, turn the denormal into zero.
    // On a VFPv2 architecture, the sign of the zero is always positive.
    if ((fpscr & FPSCR_FLUSH_TO_ZERO) != 0 && (vfp_double_type(s) & VFP_DENORMAL) != 0) {
        s->sign = 0;
        s->exponent = 0;
        s->significand = 0;
        exceptions |= FPSCR_IDC;
    }
    return exceptions;
}

// Re-pack a double-precision float. This assumes that the float is
// already normalised such that the MSB is bit 30, _not_ bit 31.
inline std::int64_t vfp_double_pack(const vfp_double* s) {
    std::uint64_t  val = ((std::uint64_t)s->sign << 48) + ((std::uint64_t)s->exponent << VFP_DOUBLE_MANTISSA_BITS) +
              (s->significand >> VFP_DOUBLE_LOW_BITS);
    return (std::int64_t)val;
}

std::uint32_t vfp_estimate_sqrt_significand(std::uint32_t exponent, std::uint32_t significand);

// A special flag to tell the normalisation code not to normalise.
#define VFP_NAN_FLAG 0x100

// A bit pattern used to indicate the initial (unset) value of the
// exception mask, in case nothing handles an instruction.  This
// doesn't include the NAN flag, which get masked out before
// we check for an error.
#define VFP_EXCEPTION_ERROR ((std::uint32_t)-1 & ~VFP_NAN_FLAG)

// A flag to tell vfp instruction type.
//  OP_SCALAR - This operation always operates in scalar mode
//  OP_SD     - The instruction exceptionally writes to a single precision result.
//  OP_DD     - The instruction exceptionally writes to a double precision result.
//  OP_SM     - The instruction exceptionally reads from a single precision operand.
enum : std::uint32_t { OP_SCALAR = (1 << 0), OP_SD = (1 << 1), OP_DD = (1 << 1), OP_SM = (1 << 2) };

struct op {
    std::uint32_t (*const fn)(ARMul_State* state, int dd, int dn, int dm, std::uint32_t fpscr);
    std::uint32_t flags;
};

inline std::uint32_t fls(std::uint32_t x) {
    int r = 32;

    if (!x)
        return 0;
    if (!(x & 0xffff0000u)) {
        x <<= 16;
        r -= 16;
    }
    if (!(x & 0xff000000u)) {
        x <<= 8;
        r -= 8;
    }
    if (!(x & 0xf0000000u)) {
        x <<= 4;
        r -= 4;
    }
    if (!(x & 0xc0000000u)) {
        x <<= 2;
        r -= 2;
    }
    if (!(x & 0x80000000u)) {
        x <<= 1;
        r -= 1;
    }
    return r;
}

std::uint32_t vfp_double_multiply(vfp_double* vdd, vfp_double* vdn, vfp_double* vdm, std::uint32_t fpscr);
std::uint32_t vfp_double_add(vfp_double* vdd, vfp_double* vdn, vfp_double* vdm, std::uint32_t fpscr);
std::uint32_t vfp_double_normaliseround(ARMul_State* state, int dd, vfp_double* vd, std::uint32_t fpscr, std::uint32_t exceptions,
                              const char* func);
