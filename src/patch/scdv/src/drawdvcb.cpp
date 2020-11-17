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

#include <Log.h>

#include "scdv/panic.h"
#include "drawdvcb.h"

TUint8 *CFbsDrawDeviceByteBuffer::GetPixelStartAddress(TInt aX, TInt aY) const {
    TInt originalX = aX;
    TInt originalY = aY;

    TransformCoordinateToPhysical(originalX, originalY, aX, aY);
    return reinterpret_cast<TUint8 *>(iBuffer) + (aX * iByteCount + aY * PhysicalScanLineBytes());
}

void CFbsDrawDeviceByteBuffer::ReadLineRaw(TInt aX, TInt aY, TInt aLength, TAny *aBuffer) const {
    // Do safety check, out of bounds
    if (aX + aLength > LongWidth())
        PanicAtTheEndOfTheWorld();

    TUint8 *pixelStart = GetPixelStartAddress(aX, aY);
    TInt increment = GetPixelIncrementUnit() * iByteCount;
    TInt iterator = 0;

    while (iterator < aLength) {
        Mem::Copy(reinterpret_cast<TUint8 *>(aBuffer) + iterator * iByteCount, pixelStart, 2);
        pixelStart += increment;
    }
}

void CFbsDrawDeviceByteBuffer::WriteRgb(TInt aX, TInt aY, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode) {
    TUint8 *pixelStart = GetPixelStartAddress(aX, aY);
    WriteRgbToAddress(pixelStart, aColor, aDrawMode);
}

void CFbsDrawDeviceByteBuffer::WriteBinary(TInt aX, TInt aY, TUint32 *aBuffer, TInt aLength, TInt aHeight, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode) {
    TUint8 *pixelAddress = NULL;
    TInt increment = GetPixelIncrementUnit() * iByteCount;

    if (aLength > 32 || aX + aLength > LongWidth())
        PanicAtTheEndOfTheWorld();

    for (TInt y = aY; y < aY + aHeight; y++) {
        pixelAddress = GetPixelStartAddress(aX, y);

        for (TInt x = aX; x < aX + aLength; x++) {
            if (*aBuffer & (1 << (x - aX))) {
                // Try to reduce if calls pls
                WriteRgbToAddress(pixelAddress, aColor, aDrawMode);
            }

            pixelAddress += increment;
        }

        aBuffer++;
    }
}

void CFbsDrawDeviceByteBuffer::WriteRgbMulti(TInt aX, TInt aY, TInt aLength, TInt aHeight, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode) {
    TUint8 *pixelAddress = NULL;
    TInt increment = GetPixelIncrementUnit() * iByteCount;

    if (aX + aLength > LongWidth())
        PanicAtTheEndOfTheWorld();

    for (TInt y = aY; y < aY + aHeight; y++) {
        pixelAddress = GetPixelStartAddress(aX, y);

        for (TInt x = aX; x < aX + aLength; x++) {
            // Try to reduce if calls pls
            WriteRgbToAddress(pixelAddress, aColor, aDrawMode);
            pixelAddress += increment;
        }
    }
}

void CFbsDrawDeviceByteBuffer::WriteRgbAlphaMulti(TInt aX, TInt aY, TInt aLength, TRgb aColor, const TUint8 *aMaskBuffer) {
    LogOut(KScdvCat, _L("Write rgb alpha multi not supported!"));
}

void CFbsDrawDeviceByteBuffer::WriteLine(TInt aX, TInt aY, TInt aLength, TUint32 *aBuffer, CGraphicsContext::TDrawMode aDrawMode) {
    TUint8 *pixelAddress = GetPixelStartAddress(aX, aY);
    TInt increment = GetPixelIncrementUnit() * iByteCount;

    TUint8 *buffer8 = reinterpret_cast<TUint8 *>(aBuffer);

    if (aX + aLength > LongWidth())
        PanicAtTheEndOfTheWorld();

    for (TInt x = aX; x < aX + aLength; x++) {
        // Try to reduce if calls pls
        WriteRgbToAddress(pixelAddress, buffer8, aDrawMode);

        pixelAddress += increment;
        buffer8 += iByteCount;
    }
}
