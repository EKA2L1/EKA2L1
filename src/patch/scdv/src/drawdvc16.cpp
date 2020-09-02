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

#include "scdv/log.h"
#include "scdv/panic.h"
#include "drawdvc16.h"

#include <gdi.h>

TRgb CFbsSixteenBitDrawDevice::ReadPixel(TInt aX, TInt aY) const {
    if ((aX < 0) || (aY < 0) || (aX >= iSize.iWidth) || (aY >= iSize.iHeight)) {
        // oops, out of bounds
        Panic(Scdv::EPanicOutOfBounds);
        return TRgb(0);
    }

    // Try to access that pixel
    TUint8 *pixelStart = GetPixelStartAddress(aX, aY);
    return TRgb::Color64K((TInt)(*reinterpret_cast<TUint16*>(pixelStart)));
}

void CFbsSixteenBitDrawDevice::WriteRgbToAddress(TUint8 *aAddress, TUint8 *aRawColor, CGraphicsContext::TDrawMode aDrawMode) {
    WriteRgbToAddress(aAddress, TRgb::Color64K(*reinterpret_cast<TUint16*>(aRawColor)), aDrawMode);
}

void CFbsSixteenBitDrawDevice::WriteRgbToAddress(TUint8 *aAddress, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode) {
    TUint16 *colorAddr = reinterpret_cast<TUint16*>(aAddress);
    TRgb currentColor = TRgb::Color64K((TInt)(*colorAddr));

    *colorAddr = static_cast<TUint16>(CFbsDrawDeviceAlgorithm::ExecuteColorDrawMode(currentColor,
        aColor, aDrawMode).Color64K());
}

void CFbsSixteenBitDrawDevice::SetSize(TSize aSize) {
    iSize = aSize;

    iLongWidth = (iSize.iWidth + 1) & ~1;
    iScanLineWords = iLongWidth >> 1;
}

TInt CFbsSixteenBitDrawDevice::Construct(TSize aSize, TInt aDataStride) {
    iDisplayMode = EColor64K;
    iByteCount = 2;

    SetSize(aSize);

    if (aDataStride != -1) {
        if (aDataStride % 2 != 0) {
            return KErrArgument;
        }

        iScanLineWords = aDataStride >> 2;
        iLongWidth = aDataStride >> 1;
    }

    TInt scanLineBufferSize = Max(iSize.iHeight, iSize.iWidth) << 1;
    iScanLineBuffer = User::Alloc(scanLineBufferSize);

    if (iScanLineBuffer == NULL) {
        return KErrNoMemory;
    }

    return KErrNone;
}
