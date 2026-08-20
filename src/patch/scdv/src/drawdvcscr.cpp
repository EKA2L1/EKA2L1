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

#define SCRDVC_IMPL(name, base)                                                   \
    void name::Update(const TRegion &aRegion) {                                   \
        UpdateScreen(1, iScreenNumber, aRegion.Count(), aRegion.RectangleList()); \
    }                                                                             \
    void name::UpdateRegion(const TRect &aRect) {                                 \
        UpdateScreen(1, iScreenNumber, 1, &aRect);                                \
    }                                                                             \
    TInt name::Construct(TUint32 aScreenNumber, TSize aSize, TInt aDataStride) {  \
        iScreenNumber = aScreenNumber;                                            \
        iDeviceOrientation = 1;                                                   \
        return base::Construct(aSize, aDataStride);                               \
    }                                                                             \
    TInt name::InitScreen() {                                                     \
        return 0;                                                                 \
    }                                                                             \
    void name::Update() {                                                         \
        TRect updateRect;                                                         \
        updateRect.iTl = TPoint(0, 0);                                            \
        updateRect.iBr = updateRect.iTl + iSize;                                  \
        UpdateScreen(1, iScreenNumber, 1, &updateRect);                           \
    }                                                                             \
    TInt name::GetInterface(TInt aInterfaceId, TAny *&aInterface) {               \
        if (aInterfaceId == KSurfaceInterfaceID) {                                \
            aInterface = static_cast<Scdv::MSurfaceId *>(this);                   \
            return KErrNone;                                                      \
        }                                                                         \
        return base::GetInterface(aInterfaceId, aInterface);                      \
    }                                                                             \
    void name::GetSurface(Scdv::TSurfaceId &aSurface) const {                     \
        aSurface.iInternal[0] = iScreenNumber;                                    \
        aSurface.iInternal[1] = 0;                                                \
        aSurface.iInternal[2] = 0x1027549A;                                       \
        aSurface.iInternal[3] = 0x01000000;                                       \
    }                                                                             \
    TUint name::DeviceOrientationsAvailable() const {                             \
        return 1 | 2 | 4 | 8;                                                     \
    }                                                                             \
    TBool name::SetDeviceOrientation(TUint aOrientation) {                        \
        TOrientation orientation;                                                 \
        switch (aOrientation) {                                                   \
        case 1: orientation = EOrientationNormal; break;                          \
        case 2: orientation = EOrientationRotate90; break;                        \
        case 4: orientation = EOrientationRotate180; break;                       \
        case 8: orientation = EOrientationRotate270; break;                       \
        default: return EFalse;                                                   \
        }                                                                         \
        if (!base::SetOrientation(orientation)) {                                 \
            return EFalse;                                                       \
        }                                                                         \
        iDeviceOrientation = aOrientation;                                        \
        return ETrue;                                                             \
    }                                                                             \
    TUint name::DeviceOrientation() const {                                       \
        return iDeviceOrientation;                                                \
    }

SCRDVC_IMPL(CFbsTwelveBitScreenDrawDevice, CFbsTwelveBitDrawDevice)
SCRDVC_IMPL(CFbsSixteenBitScreenDrawDevice, CFbsSixteenBitDrawDevice)
SCRDVC_IMPL(CFbsTwentyfourBitAlphaScreenDrawDevice, CFbsTwentyfourBitAlphaDrawDevice)
SCRDVC_IMPL(CFbsTwentyfourBitUnsignedByteScreenDrawDevice, CFbsTwentyfourBitUnsignedByteDrawDevice)
