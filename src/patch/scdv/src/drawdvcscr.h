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

#define SCRDVC_DECL()                                                     \
    TUint32 iScreenNumber;                                                \
    TUint iDeviceOrientation;                                             \
                                                                          \
public:                                                                   \
    TInt Construct(TUint32 aScreenNumber, TSize aSize, TInt aDataStride); \
    virtual TInt InitScreen();                                            \
    virtual void Update();                                                \
    virtual void Update(const TRegion &aRegion);                          \
    virtual void UpdateRegion(const TRect &aRect);                        \
    virtual TInt GetInterface(TInt aInterfaceId, TAny *&aInterface);      \
    virtual void GetSurface(Scdv::TSurfaceId &aSurface) const;            \
    virtual TUint DeviceOrientationsAvailable() const;                    \
    virtual TBool SetDeviceOrientation(TUint aOrientation);               \
    virtual TUint DeviceOrientation() const

class CFbsTwelveBitScreenDrawDevice : public CFbsTwelveBitDrawDevice, public Scdv::MSurfaceId {
    SCRDVC_DECL();
};

class CFbsSixteenBitScreenDrawDevice : public CFbsSixteenBitDrawDevice, public Scdv::MSurfaceId {
    SCRDVC_DECL();
};

class CFbsTwentyfourBitAlphaScreenDrawDevice : public CFbsTwentyfourBitAlphaDrawDevice, public Scdv::MSurfaceId {
    SCRDVC_DECL();
};

class CFbsTwentyfourBitUnsignedByteScreenDrawDevice : public CFbsTwentyfourBitUnsignedByteDrawDevice, public Scdv::MSurfaceId {
    SCRDVC_DECL();
};

#endif
