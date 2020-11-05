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

#ifndef SCDVC_DRAW_SCR_DVC_H_
#define SCDVC_DRAW_SCR_DVC_H_

#include <e32std.h>

#include "drawdvc12.h"
#include "drawdvc16.h"
#include "drawdvc32.h"

#define SCRDVC_DECL()                                                       \
    TUint32 iScreenNumber;                                                  \
public:                                                                     \
    TInt Construct(TUint32 aScreenNumber, TSize aSize, TInt aDataStride);   \
    virtual TInt InitScreen();                                              \
    virtual void Update();                                                  \
    virtual void Update(const TRegion &aRegion);                            \
    virtual void UpdateRegion(const TRect &aRect)                          

class CFbsTwelveBitScreenDrawDevice : public CFbsTwelveBitDrawDevice {
    SCRDVC_DECL();
};

class CFbsSixteenBitScreenDrawDevice : public CFbsSixteenBitDrawDevice {
    SCRDVC_DECL();
};

#ifdef EKA2
class CFbsTwentyfourBitAlphaScreenDrawDevice : public CFbsTwentyfourBitAlphaDrawDevice {
    SCRDVC_DECL();
};

class CFbsTwentyfourBitUnsignedByteScreenDrawDevice : public CFbsTwentyfourBitUnsignedByteDrawDevice {
    SCRDVC_DECL();
};
#endif

#endif
