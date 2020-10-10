/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#ifndef PCOMMON_INT_H_
#define PCOMMON_INT_H_

#include <e32def.h>

#ifndef EKA2
inline TInt64 MakeSoftwareInt64FromHardwareUint64(const TUint64 aOriginal) {
    return TInt64(static_cast<TUint>(aOriginal >> 32), static_cast<TUint>(aOriginal));
}

inline TUint64 MakeHardwareUint64FromSoftwareInt64(const TInt64 aOriginal) {
    return (aOriginal.High() << 32) | aOriginal.Low();
}
#endif

#endif
