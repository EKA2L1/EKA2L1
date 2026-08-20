/*
 * Copyright (c) 2026 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
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

// The rotated ScanLineBytes()/LongWidth() paths divide by a runtime value, so
// the compiler calls the ARM EABI division helpers. Newer SDKs export those
// from drtaeabi.dll at ordinals 222 and 223, and their import library is
// searched before libgcc, so a link against such an SDK turns them into ROM
// imports. No firmware this replacement DLL must run on exports more than 221
// ordinals: the two helpers were appended to the frozen DEF afterwards, and the
// unresolvable import would leave the call veneer branching to address zero.
//
// Defining them here keeps them resolved from this object file, ahead of every
// library on the link line, whichever SDK builds the DLL.

#include <e32def.h>

extern "C" TUint __aeabi_uidiv(TUint aNumerator, TUint aDenominator) {
    if (aDenominator == 0) {
        return 0;
    }

    TUint quotient = 0;
    TUint remainder = 0;

    // Restoring division. The remainder stays below the divisor and gains one
    // numerator bit per step, so it is always under 2^31 when shifted.
    for (TInt bit = 31; bit >= 0; bit--) {
        remainder = (remainder << 1) | ((aNumerator >> bit) & 1);
        quotient <<= 1;

        if (remainder >= aDenominator) {
            remainder -= aDenominator;
            quotient |= 1;
        }
    }

    return quotient;
}

extern "C" TInt __aeabi_idiv(TInt aNumerator, TInt aDenominator) {
    const TBool negateResult = ((aNumerator ^ aDenominator) < 0);

    const TUint quotient = __aeabi_uidiv(
        static_cast<TUint>(aNumerator < 0 ? -aNumerator : aNumerator),
        static_cast<TUint>(aDenominator < 0 ? -aDenominator : aDenominator));

    return negateResult ? -static_cast<TInt>(quotient) : static_cast<TInt>(quotient);
}
