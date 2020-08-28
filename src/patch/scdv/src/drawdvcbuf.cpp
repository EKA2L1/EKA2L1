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

#include "drawdvcbuf.h"
#include "scdv/log.h"
#include "scdv/sv.h"

CFbsDrawDeviceBuffer::CFbsDrawDeviceBuffer()
    : iBuffer(NULL)
    , iScanLineBuffer(NULL)
    , iBlackMap(128)
    , iWhiteMap(255)
    , iSize(0, 0)
    , iDisplayMode(ERgb) {
}

CFbsDrawDeviceBuffer::~CFbsDrawDeviceBuffer() {
    if (iScanLineBuffer) {
        User::Free(iScanLineBuffer);
    }
}

TDisplayMode CFbsDrawDeviceBuffer::DisplayMode() const {
    return iDisplayMode;
}

TUint32 *CFbsDrawDeviceBuffer::ScanLineBuffer() const {
    return reinterpret_cast<TUint32 *>(iScanLineBuffer);
}

TDisplayMode CFbsDrawDeviceBuffer::ScanLineDisplayMode() const {
    return iDisplayMode;
}

TInt CFbsDrawDeviceBuffer::InitScreen() {
    Scdv::Log("Init screen called on unsupported draw device!");
    return KErrNotSupported;
}

void CFbsDrawDeviceBuffer::SetBits(TAny *aBits) {
    iBuffer = aBits;
}

void CFbsDrawDeviceBuffer::SetAutoUpdate(TBool aValue) {
    Scdv::Log("Auto update not supported!");
}

void CFbsDrawDeviceBuffer::SetDisplayMode(CFbsDrawDevice *aDrawDevice) {
    Scdv::Log("Set display mode on draw device base has no effect");
}

void CFbsDrawDeviceBuffer::SetDitherOrigin(const TPoint &aDitherOrigin) {
    iDitherOrigin = aDitherOrigin;
}

void CFbsDrawDeviceBuffer::SetUserDisplayMode(TDisplayMode) {
}

void CFbsDrawDeviceBuffer::SetShadowMode(TShadowMode) {
}

void CFbsDrawDeviceBuffer::SetFadingParameters(TUint8 aBlackMap, TUint8 aWhiteMap) {
    iBlackMap = aBlackMap;
    iWhiteMap = aWhiteMap;
}

void CFbsDrawDeviceBuffer::ShadowArea(const TRect &) {
}

void CFbsDrawDeviceBuffer::ShadowBuffer(TInt, TUint32 *) {
}

void CFbsDrawDeviceBuffer::Update() {
}

void CFbsDrawDeviceBuffer::Update(const TRegion &) {
}

void CFbsDrawDeviceBuffer::UpdateRegion(const TRect &) {
}

TInt CFbsDrawDeviceBuffer::SetCustomPalette(const CPalette *) {
    return KErrNotSupported;
}

TInt CFbsDrawDeviceBuffer::GetCustomPalette(CPalette *&) {
    return KErrNotSupported;
}

TSize CFbsDrawDeviceBuffer::SizeInPixels() const {
    return iSize;
}

TBool CFbsDrawDeviceBuffer::SetOrientation(TOrientation aOri) {
    if (aOri > EOrientationRotate270 || aOri < EOrientationNormal)
        return EFalse;

    Scdv::Log("Orientation set to %d", aOri);
    iOrientation = aOri;
    return ETrue;
}

void CFbsDrawDeviceBuffer::GetDrawRect(TRect &aDrawRect) const {
    aDrawRect.iTl = TPoint(0, 0);

    if (iOrientation & 1) {
        aDrawRect.iBr.iX = aDrawRect.iTl.iX + iSize.iHeight;
        aDrawRect.iBr.iY = aDrawRect.iTl.iY + iSize.iWidth;
    } else {
        aDrawRect.iBr = aDrawRect.iTl + iSize;
    }
}

TInt CFbsDrawDeviceBuffer::WriteBitmapBlock(const TPoint &aDest,
    const TUint32 *aSrcBase, TInt aSrcStride, const TSize &aSrcSize,
    const TRect &aSrcRect) {
    if (Orientation() != EOrientationNormal) {
        return KErrNotSupported;
    }

    TFastBlitInfo info;
    info.iDestBase = reinterpret_cast<TUint8 *>(iBuffer);
    info.iSrcBase = reinterpret_cast<const TUint8 *>(aSrcBase);
    info.iDestPoint = aDest;
    info.iDestStride = ScanLineBytes();
    info.iSourceStride = aSrcStride;
    info.iSrcSize = aSrcSize;
    info.iSrcRect = aSrcRect;

    FastBlit(2, &info);

    return KErrNone;
}

TInt CFbsDrawDeviceBuffer::WriteBitmapBlock(const TPoint &aDest, CFbsDrawDevice *aSrcDrawDevice,
    const TRect &aSrcRect) {
    TAny *blitBlock = NULL;
    TInt result = aSrcDrawDevice->GetInterface(KFastBlit2InterfaceID, blitBlock);

    if (result != KErrNone) {
        return result;
    }

    TAny *orientation = NULL;
    result = aSrcDrawDevice->GetInterface(KOrientationInterfaceID, orientation);

    if (result != KErrNone) {
        return result;
    }

    if (((MOrientation *)orientation)->Orientation() != EOrientationNormal) {
        return KErrNotSupported;
    }

    if (aSrcDrawDevice->DisplayMode() != DisplayMode()) {
        return KErrNotSupported;
    }

    return WriteBitmapBlock(aDest, ((MFastBlitBlock *)blitBlock)->Bits(), aSrcDrawDevice->ScanLineBytes(), aSrcDrawDevice->SizeInPixels(),
        aSrcRect);
}

const TUint32 *CFbsDrawDeviceBuffer::Bits() const {
    return reinterpret_cast<const TUint32 *>(iBuffer);
}
