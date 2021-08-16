// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

// Detect the cpu, so we'll know which optimizations to use
#pragma once

#include <common/platform.h>
#include <string>

namespace eka2l1::common {
    enum cpu_vendor {
        VENDOR_INTEL = 0,
        VENDOR_AMD = 1,
        VENDOR_ARM = 2,
        VENDOR_OTHER = 3,
    };

    struct cpu_info {
        cpu_vendor vendor;

        char cpu_string[0x21];
        char brand_string[0x41];
        bool OS64bit;
        bool CPU64bit;
        bool Mode64bit;

        bool HTT;
        int num_cores;
        int logical_cpu_count;

        bool bSSE;
        bool bSSE2;
        bool bSSE3;
        bool bSSSE3;
        bool bPOPCNT;
        bool bSSE4_1;
        bool bSSE4_2;
        bool bLZCNT;
        bool bSSE4A;
        bool bAVX;
        bool bAVX2;
        bool bFMA;
        bool bAES;
        bool bLAHFSAHF64;
        bool bLongMode;
        bool bAtom;
        bool bBMI1;
        bool bBMI2;
        bool bMOVBE;
        bool bFXSR;

        // ARM specific CPUInfo
        bool bSwp;
        bool bHalf;
        bool bThumb;
        bool bFastMult;
        bool bVFP;
        bool bEDSP;
        bool bThumbEE;
        bool bNEON;
        bool bVFPv3;
        bool bTLS;
        bool bVFPv4;
        bool bIDIVa;
        bool bIDIVt;
        bool bARMv7;

        // ARMv8 specific
        bool bFP;
        bool bASIMD;

        // MIPS specific
        bool bXBurst1;
        bool bXBurst2;

        // Quirks
        struct {
            // Samsung Galaxy S7 devices (Exynos 8890) have a big.LITTLE configuration where the cacheline size differs between big and LITTLE.
            // GCC's cache clearing function would detect the cacheline size on one and keep it for later. When clearing
            // with the wrong cacheline size on the other, that's an issue. In case we want to do something different in this
            // situation in the future, let's keep this as a quirk, but our current code won't detect it reliably
            // if it happens on new archs. We now use better clearing code on ARM64 that doesn't have this issue.
            bool bExynos8890DifferingCachelineSizes;
        } sQuirks;

        // Call Detect()
        explicit cpu_info() = default;

        // Detects the various cpu features
        void detect();

        // Turn the cpu info into a string we can show
        // std::string summarize();
    };
}