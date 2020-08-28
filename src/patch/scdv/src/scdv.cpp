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

#include "scdv/draw.h"

#if defined(EKA2) || defined(__SERIES80__)
TDisplayMode CFbsDrawDevice::DisplayMode16M() {
#ifdef EKA2
    return EColor16MA;
#else
    return EColor16M;
#endif
}
#endif

#ifndef EKA2
EXPORT_C TInt E32Dll(TDllReason reason) {
    return KErrNone;
}
#endif