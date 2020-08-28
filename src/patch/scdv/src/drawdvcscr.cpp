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

#include "drawdvcscr.h"
#include "scdv/sv.h"

///////////////////////////////////////////////
//
//	RGBA screen draw device
//
//////////////////////////////////////////////

#define SCRDVC_IMPL(name, base)                                                     \
    void name::Update(const TRegion &aRegion) {                                     \
        UpdateScreen(1, iScreenNumber, aRegion.Count(), aRegion.RectangleList());   \
    }                                                                               \
    void name::UpdateRegion(const TRect &aRect) {                                   \
        UpdateScreen(1, iScreenNumber, 1, &aRect);                                  \
    }                                                                               \
    TInt name::Construct(TUint32 aScreenNumber, TSize aSize, TInt aDataStride) {    \
        iScreenNumber = aScreenNumber;                                              \
        return base::Construct(aSize, aDataStride);                                 \
    }                                                                               \
    TInt name::InitScreen() {                                                       \
        return 0;                                                                   \
    }                                                                               \
    void name::Update() {                                                           \
        TRect updateRect;                                                           \
        updateRect.iTl = TPoint(0, 0);                                              \
        updateRect.iBr = updateRect.iTl + iSize;                                    \
        UpdateScreen(1, iScreenNumber, 1, &updateRect);                             \
    }

SCRDVC_IMPL(CFbsSixteenBitScreenDrawDevice, CFbsSixteenBitDrawDevice)

#ifdef EKA2
SCRDVC_IMPL(CFbsTwentyfourBitAlphaScreenDrawDevice, CFbsTwentyfourBitAlphaDrawDevice)
SCRDVC_IMPL(CFbsTwentyfourBitUnsignedByteScreenDrawDevice, CFbsTwentyfourBitUnsignedByteDrawDevice)
#endif
