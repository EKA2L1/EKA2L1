// Three registers of the same length
INST(asimd_VHADD,           "VHADD",                    "1111001U0Dzznnnndddd0000NQM0mmmm") // ASIMD
INST(asimd_VQADD,           "VQADD",                    "1111001U0Dzznnnndddd0000NQM1mmmm") // ASIMD
INST(asimd_VRHADD,          "VRHADD",                   "1111001U0Dzznnnndddd0001NQM0mmmm") // ASIMD
INST(asimd_VAND_reg,        "VAND (register)",          "111100100D00nnnndddd0001NQM1mmmm") // ASIMD
INST(asimd_VBIC_reg,        "VBIC (register)",          "111100100D01nnnndddd0001NQM1mmmm") // ASIMD
INST(asimd_VORR_reg,        "VORR (register)",          "111100100D10nnnndddd0001NQM1mmmm") // ASIMD
INST(asimd_VORN_reg,        "VORN (register)",          "111100100D11nnnndddd0001NQM1mmmm") // ASIMD
INST(asimd_VEOR_reg,        "VEOR (register)",          "111100110D00nnnndddd0001NQM1mmmm") // ASIMD
INST(asimd_VBSL,            "VBSL",                     "111100110D01nnnndddd0001NQM1mmmm") // ASIMD
INST(asimd_VBIT,            "VBIT",                     "111100110D10nnnndddd0001NQM1mmmm") // ASIMD
INST(asimd_VBIF,            "VBIF",                     "111100110D11nnnndddd0001NQM1mmmm") // ASIMD
INST(asimd_VHSUB,           "VHSUB",                    "1111001U0Dzznnnndddd0010NQM0mmmm") // ASIMD
INST(asimd_VQSUB,           "VQSUB",                    "1111001U0Dzznnnndddd0010NQM1mmmm") // ASIMD
INST(asimd_VCGT_reg,        "VCGT (register)",          "1111001U0Dzznnnndddd0011NQM0mmmm") // ASIMD
INST(asimd_VCGE_reg,        "VCGE (register)",          "1111001U0Dzznnnndddd0011NQM1mmmm") // ASIMD
INST(asimd_VSHL_reg,        "VSHL (register)",          "1111001U0Dzznnnndddd0100NQM0mmmm") // ASIMD
INST(asimd_VQSHL_reg,       "VQSHL (register)",         "1111001U0Dzznnnndddd0100NQM1mmmm") // ASIMD
INST(asimd_VRSHL,           "VRSHL",                    "1111001U0Dzznnnndddd0101NQM0mmmm") // ASIMD
//INST(asimd_VQRSHL,          "VQRSHL",                   "1111001U0-CC--------0101---1----") // ASIMD
INST(asimd_VMAX,            "VMAX/VMIN (integer)",      "1111001U0Dzznnnnmmmm0110NQMommmm") // ASIMD
INST(asimd_VABD,            "VABD",                     "1111001U0Dzznnnndddd0111NQM0mmmm") // ASIMD
INST(asimd_VABA,            "VABA",                     "1111001U0Dzznnnndddd0111NQM1mmmm") // ASIMD
INST(asimd_VADD_int,        "VADD (integer)",           "111100100Dzznnnndddd1000NQM0mmmm") // ASIMD
INST(asimd_VSUB_int,        "VSUB (integer)",           "111100110Dzznnnndddd1000NQM0mmmm") // ASIMD
INST(asimd_VTST,            "VTST",                     "111100100Dzznnnndddd1000NQM1mmmm") // ASIMD
INST(asimd_VCEQ_reg,        "VCEG (register)",          "111100110Dzznnnndddd1000NQM1mmmm") // ASIMD
INST(asimd_VMLA,            "VMLA/VMLS",                "1111001o0Dzznnnndddd1001NQM0mmmm") // ASIMD
INST(asimd_VMUL,            "VMUL",                     "1111001P0Dzznnnndddd1001NQM1mmmm") // ASIMD
INST(asimd_VPMAX_int,       "VPMAX/VPMIN (integer)",    "1111001U0Dzznnnndddd1010NQMommmm") // ASIMD
INST(asimd_VQDMULH,         "VQDMULH",                  "111100100Dzznnnndddd1011NQM0mmmm") // ASIMD
INST(asimd_VQRDMULH,        "VQRDMULH",                 "111100110Dzznnnndddd1011NQM0mmmm") // ASIMD
INST(asimd_VPADD,           "VPADD",                    "111100100Dzznnnndddd1011NQM1mmmm") // ASIMD
INST(asimd_VFMA,            "VFMA",                     "111100100D0znnnndddd1100NQM1mmmm") // ASIMD
INST(asimd_VFMS,            "VFMS",                     "111100100D1znnnndddd1100NQM1mmmm") // ASIMD
INST(asimd_VADD_float,      "VADD (floating-point)",    "111100100D0znnnndddd1101NQM0mmmm") // ASIMD
INST(asimd_VSUB_float,      "VSUB (floating-point)",    "111100100D1znnnndddd1101NQM0mmmm") // ASIMD
INST(asimd_VPADD_float,     "VPADD (floating-point)",   "111100110D0znnnndddd1101NQM0mmmm") // ASIMD
INST(asimd_VABD_float,      "VABD (floating-point)",    "111100110D1znnnndddd1101NQM0mmmm") // ASIMD
INST(asimd_VMLA_float,      "VMLA (floating-point)",    "111100100D0znnnndddd1101NQM1mmmm") // ASIMD
INST(asimd_VMLS_float,      "VMLS (floating-point)",    "111100100D1znnnndddd1101NQM1mmmm") // ASIMD
INST(asimd_VMUL_float,      "VMUL (floating-point)",    "111100110D0znnnndddd1101NQM1mmmm") // ASIMD
INST(asimd_VCEQ_reg_float,  "VCEQ (register)",          "111100100D0znnnndddd1110NQM0mmmm") // ASIMD
INST(asimd_VCGE_reg_float,  "VCGE (register)",          "111100110D0znnnndddd1110NQM0mmmm") // ASIMD
INST(asimd_VCGT_reg_float,  "VCGT (register)",          "111100110D1znnnndddd1110NQM0mmmm") // ASIMD
INST(asimd_VACGE,           "VACGE",                    "111100110Doznnnndddd1110NQM1mmmm") // ASIMD
INST(asimd_VMAX_float,      "VMAX (floating-point)",    "111100100D0znnnndddd1111NQM0mmmm") // ASIMD
INST(asimd_VMIN_float,      "VMIN (floating-point)",    "111100100D1znnnndddd1111NQM0mmmm") // ASIMD
INST(asimd_VPMAX_float,     "VPMAX (floating-point)",   "111100110D0znnnndddd1111NQM0mmmm") // ASIMD
INST(asimd_VPMIN_float,     "VPMIN (floating-point)",   "111100110D1znnnndddd1111NQM0mmmm") // ASIMD
INST(asimd_VRECPS,          "VRECPS",                   "111100100D0znnnndddd1111NQM1mmmm") // ASIMD
INST(asimd_VRSQRTS,         "VRSQRTS",                  "111100100D1znnnndddd1111NQM1mmmm") // ASIMD

// Three registers of different lengths
INST(asimd_VADDL,           "VADDL/VADDW",              "1111001U1Dzznnnndddd000oN0M0mmmm") // ASIMD
INST(asimd_VSUBL,           "VSUBL/VSUBW",              "1111001U1Dzznnnndddd001oN0M0mmmm") // ASIMD
//INST(asimd_VADDHN,          "VADDHN",                   "111100101-----------0100-0-0----") // ASIMD
//INST(asimd_VRADDHN,         "VRADDHN",                  "111100111-----------0100-0-0----") // ASIMD
INST(asimd_VABAL,           "VABAL",                    "1111001U1Dzznnnndddd0101N0M0mmmm") // ASIMD
//INST(asimd_VSUBHN,          "VSUBHN",                   "111100101-----------0110-0-0----") // ASIMD
//INST(asimd_VRSUBHN,         "VRSUBHN",                  "111100111-----------0110-0-0----") // ASIMD
INST(asimd_VABDL,           "VABDL",                    "1111001U1Dzznnnndddd0111N0M0mmmm") // ASIMD
INST(asimd_VMLAL,           "VMLAL/VMLSL",              "1111001U1Dzznnnndddd10o0N0M0mmmm") // ASIMD
//INST(asimd_VQDMLAL,         "VQDMLAL",                  "111100101-----------10-1-0-0----") // ASIMD
INST(asimd_VMULL,           "VMULL",                    "1111001U1Dzznnnndddd11P0N0M0mmmm") // ASIMD
//INST(asimd_VQDMULL,         "VQDMULL",                  "111100101-----------1101-0-0----") // ASIMD

// Two registers and a scalar
INST(asimd_VMLA_scalar,     "VMLA (scalar)",            "1111001Q1Dzznnnndddd0o0FN1M0mmmm") // ASIMD
INST(asimd_VMLAL_scalar,    "VMLAL (scalar)",           "1111001U1dzznnnndddd0o10N1M0mmmm") // ASIMD
//INST(asimd_VQDMLAL_scalar,  "VQDMLAL/VQDMLSL (scalar)", "111100101-BB--------0x11-1-0----") // ASIMD
INST(asimd_VMUL_scalar,     "VMUL (scalar)",            "1111001Q1Dzznnnndddd100FN1M0mmmm") // ASIMD
INST(asimd_VMULL_scalar,    "VMULL (scalar)",           "1111001U1Dzznnnndddd1010N1M0mmmm") // ASIMD
INST(asimd_VQDMULL_scalar,  "VQDMULL (scalar)",         "111100101Dzznnnndddd1011N1M0mmmm") // ASIMD
INST(asimd_VQDMULH_scalar,  "VQDMULH (scalar)",         "1111001Q1Dzznnnndddd1100N1M0mmmm") // ASIMD
INST(asimd_VQRDMULH_scalar, "VQRDMULH (scalar)",        "1111001Q1Dzznnnndddd1101N1M0mmmm") // ASIMD

// Two registers and a shift amount
INST(asimd_SHR,             "SHR",                      "1111001U1Diiiiiidddd0000LQM1mmmm") // ASIMD
INST(asimd_SRA,             "SRA",                      "1111001U1Diiiiiidddd0001LQM1mmmm") // ASIMD
INST(asimd_VRSHR,           "VRSHR",                    "1111001U1Diiiiiidddd0010LQM1mmmm") // ASIMD
INST(asimd_VRSRA,           "VRSRA",                    "1111001U1Diiiiiidddd0011LQM1mmmm") // ASIMD
INST(asimd_VSRI,            "VSRI",                     "111100111Diiiiiidddd0100LQM1mmmm") // ASIMD
INST(asimd_VSHL,            "VSHL",                     "111100101Diiiiiidddd0101LQM1mmmm") // ASIMD
INST(asimd_VSLI,            "VSLI",                     "111100111Diiiiiidddd0101LQM1mmmm") // ASIMD
INST(asimd_VQSHL,           "VQSHL" ,                   "1111001U1Diiiiiidddd011oLQM1mmmm") // ASIMD
INST(asimd_VSHRN,           "VSHRN",                    "111100101Diiiiiidddd100000M1mmmm") // ASIMD
INST(asimd_VRSHRN,          "VRSHRN",                   "111100101Diiiiiidddd100001M1mmmm") // ASIMD
INST(asimd_VQSHRUN,         "VQSHRUN",                  "111100111Diiiiiidddd100000M1mmmm") // ASIMD
INST(asimd_VQRSHRUN,        "VQRSHRUN",                 "111100111Diiiiiidddd100001M1mmmm") // ASIMD
INST(asimd_VQSHRN,          "VQSHRN",                   "1111001U1Diiiiiidddd100100M1mmmm") // ASIMD
INST(asimd_VQRSHRN,         "VQRSHRN",                  "1111001U1Diiiiiidddd100101M1mmmm") // ASIMD
INST(asimd_VSHLL,           "VSHLL",                    "1111001U1Diiiiiidddd101000M1mmmm") // ASIMD
INST(asimd_VCVT_fixed,      "VCVT (fixed-point)",       "1111001U1Diiiiiidddd111o0QM1mmmm") // ASIMD

// Two registers, miscellaneous
INST(asimd_VREV,            "VREV{16,32,64}",           "111100111D11zz00dddd000ooQM0mmmm") // ASIMD
INST(asimd_VPADDL,          "VPADDL",                   "111100111D11zz00dddd0010oQM0mmmm") // ASIMD
INST(asimd_VCLS,            "VCLS",                     "111100111D11zz00dddd01000QM0mmmm") // ASIMD
INST(asimd_VCLZ,            "VCLZ",                     "111100111D11zz00dddd01001QM0mmmm") // ASIMD
INST(asimd_VCNT,            "VCNT",                     "111100111D11zz00dddd01010QM0mmmm") // ASIMD
INST(asimd_VMVN_reg,        "VMVN_reg",                 "111100111D11zz00dddd01011QM0mmmm") // ASIMD
INST(asimd_VPADAL,          "VPADAL",                   "111100111D11zz00dddd0110oQM0mmmm") // ASIMD
INST(asimd_VQABS,           "VQABS",                    "111100111D11zz00dddd01110QM0mmmm") // ASIMD
INST(asimd_VQNEG,           "VQNEG",                    "111100111D11zz00dddd01111QM0mmmm") // ASIMD
INST(asimd_VCGT_zero,       "VCGT (zero)",              "111100111D11zz01dddd0F000QM0mmmm") // ASIMD
INST(asimd_VCGE_zero,       "VCGE (zero)",              "111100111D11zz01dddd0F001QM0mmmm") // ASIMD
INST(asimd_VCEQ_zero,       "VCEQ (zero)",              "111100111D11zz01dddd0F010QM0mmmm") // ASIMD
INST(asimd_VCLE_zero,       "VCLE (zero)",              "111100111D11zz01dddd0F011QM0mmmm") // ASIMD
INST(asimd_VCLT_zero,       "VCLT (zero)",              "111100111D11zz01dddd0F100QM0mmmm") // ASIMD
INST(asimd_VABS,            "VABS",                     "111100111D11zz01dddd0F110QM0mmmm") // ASIMD
INST(asimd_VNEG,            "VNEG",                     "111100111D11zz01dddd0F111QM0mmmm") // ASIMD
INST(asimd_VSWP,            "VSWP",                     "111100111D110010dddd00000QM0mmmm") // ASIMD
INST(arm_UDF,               "UNALLOCATED",              "111100111-11--10----00000--0----") // ASIMD
INST(asimd_VTRN,            "VTRN",                     "111100111D11zz10dddd00001QM0mmmm") // ASIMD
INST(asimd_VUZP,            "VUZP",                     "111100111D11zz10dddd00010QM0mmmm") // ASIMD
INST(asimd_VZIP,            "VZIP",                     "111100111D11zz10dddd00011QM0mmmm") // ASIMD
INST(asimd_VMOVN,           "VMOVN",                    "111100111D11zz10dddd001000M0mmmm") // ASIMD
INST(asimd_VQMOVUN,         "VQMOVUN",                  "111100111D11zz10dddd001001M0mmmm") // ASIMD
INST(asimd_VQMOVN,          "VQMOVN",                   "111100111D11zz10dddd00101oM0mmmm") // ASIMD
INST(asimd_VSHLL_max,       "VSHLL_max",                "111100111D11zz10dddd001100M0mmmm") // ASIMD
INST(arm_UDF,               "UNALLOCATED (VRINTN)",     "111100111-11--10----01000--0----")
INST(arm_UDF,               "UNALLOCATED (VRINTX)",     "111100111-11--10----01001--0----")
INST(arm_UDF,               "UNALLOCATED (VRINTA)",     "111100111-11--10----01010--0----")
INST(arm_UDF,               "UNALLOCATED (VRINTZ)",     "111100111-11--10----01011--0----")
INST(arm_UDF,               "UNALLOCATED (VRINTM)",     "111100111-11--10----01101--0----")
INST(arm_UDF,               "UNALLOCATED (VRINTP)",     "111100111-11--10----01111--0----")
INST(arm_UDF,               "UNALLOCATED (VCVT half)",  "111100111-11--10----011-00-0----") // ASIMD
INST(arm_UDF,               "UNALLOCATED",              "111100111-11--10----011-01-0----") // ASIMD
INST(arm_UDF,               "UNALLOCATED (VCVTA)",      "111100111-11--11----0000---0----")
INST(arm_UDF,               "UNALLOCATED (VCVTN)",      "111100111-11--11----0001---0----")
INST(arm_UDF,               "UNALLOCATED (VCVTP)",      "111100111-11--11----0010---0----")
INST(arm_UDF,               "UNALLOCATED (VCVTM)",      "111100111-11--11----0011---0----")
INST(asimd_VRECPE,          "VRECPE",                   "111100111D11zz11dddd010F0QM0mmmm") // ASIMD
INST(asimd_VRSQRTE,         "VRSQRTE",                  "111100111D11zz11dddd010F1QM0mmmm") // ASIMD
INST(asimd_VCVT_integer,    "VCVT (integer)",           "111100111D11zz11dddd011oUQM0mmmm") // ASIMD

// One register and modified immediate
INST(asimd_VMOV_imm,        "VBIC, VMOV, VMVN, VORR (immediate)",  "1111001a1D000bcdVVVVmmmm0Qo1efgh") // ASIMD

// Miscellaneous
INST(asimd_VEXT,            "VEXT",                     "111100101D11nnnnddddiiiiNQM0mmmm") // ASIMD
INST(asimd_VTBL,            "VTBL",                     "111100111D11nnnndddd10zzN0M0mmmm") // ASIMD
INST(asimd_VTBX,            "VTBX",                     "111100111D11nnnndddd10zzN1M0mmmm") // ASIMD
INST(asimd_VDUP_scalar,     "VDUP (scalar)",            "111100111D11iiiidddd11000QM0mmmm") // ASIMD
INST(arm_UDF,               "UNALLOCATED",              "111100111-11--------11-----0----") // ASIMD