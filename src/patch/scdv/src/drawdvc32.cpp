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

#include <scdv/log.h>
#include <scdv/panic.h>
#include <scdv/sv.h>

#include "drawdvc32.h"

TUint8 *CFbsThirtyTwoBitsDrawDevice::GetPixelStartAddress(TInt aX, TInt aY) const {
    TInt originalX = aX;
    TInt originalY = aY;

    TransformCoordinateToPhysical(originalX, originalY, aX, aY);
    return reinterpret_cast<TUint8 *>(iBuffer) + (aX * 4 + aY * PhysicalScanLineBytes());
}

TRgb CFbsThirtyTwoBitsDrawDevice::ReadPixel(TInt aX, TInt aY) const {
    if ((aX < 0) || (aY < 0) || (aX >= iSize.iWidth) || (aY >= iSize.iHeight)) {
        // oops, out of bounds
        Panic(Scdv::EPanicOutOfBounds);
        return TRgb(0);
    }

    // Try to access that pixel
    TUint8 *pixelStart = GetPixelStartAddress(aX, aY);
    return TRgb(pixelStart[0], pixelStart[1], pixelStart[2], pixelStart[3]);
}

void CFbsThirtyTwoBitsDrawDevice::ReadLineRaw(TInt aX, TInt aY, TInt aLength, TAny *aBuffer) const {
    // Do safety check, out of bounds
    if (aX + aLength > LongWidth())
        PanicAtTheEndOfTheWorld();

    TUint8 *pixelStart = GetPixelStartAddress(aX, aY);
    TInt increment = GetPixelIncrementUnit() * 4;
    TInt iterator = 0;

    while (iterator < aLength) {
        Mem::Copy(reinterpret_cast<TUint8 *>(aBuffer) + iterator * 4, pixelStart, 4);
        pixelStart += increment;
    }
}

// Write functions
static void WriteRgb32ToAddressAlpha(TUint8 *aAddress, TUint8 aRed, TUint8 aGreen, TUint8 aBlue, TUint8 aAlpha) {
    aAddress[0] = aRed;
    aAddress[1] = aGreen;
    aAddress[2] = aBlue;
    aAddress[3] = aAlpha;
}

static void WriteRgb32ToAddressAND(TUint8 *aAddress, TUint8 aRed, TUint8 aGreen, TUint8 aBlue, TUint8 aAlpha) {
    aAddress[0] &= aRed;
    aAddress[1] &= aGreen;
    aAddress[2] &= aBlue;
    aAddress[3] &= aAlpha;
}

static void WriteRgb32ToAddressOR(TUint8 *aAddress, TUint8 aRed, TUint8 aGreen, TUint8 aBlue, TUint8 aAlpha) {
    aAddress[0] |= aRed;
    aAddress[1] |= aGreen;
    aAddress[2] |= aBlue;
    aAddress[3] |= aAlpha;
}

static void WriteRgb32ToAddressXOR(TUint8 *aAddress, TUint8 aRed, TUint8 aGreen, TUint8 aBlue, TUint8 aAlpha) {
    aAddress[0] ^= aRed;
    aAddress[1] ^= aGreen;
    aAddress[2] ^= aBlue;
    aAddress[3] ^= aAlpha;
}

static void WriteRgb32ToAddressNOTSC(TUint8 *aAddress, TUint8 aRed, TUint8 aGreen, TUint8 aBlue, TUint8 aAlpha) {
    aAddress[0] = ~(aAddress[0]);
    aAddress[1] = ~(aAddress[1]);
    aAddress[2] = ~(aAddress[2]);
    aAddress[3] = ~(aAddress[3]);
}

static void WriteRgb32ToAddressANDNOT(TUint8 *aAddress, TUint8 aRed, TUint8 aGreen, TUint8 aBlue, TUint8 aAlpha) {
    aAddress[0] = (~aAddress[0]) & aRed;
    aAddress[1] = (~aAddress[1]) & aGreen;
    aAddress[2] = (~aAddress[2]) & aBlue;
    aAddress[3] = (~aAddress[3]) & aAlpha;
}

static void WriteRgb32ToAddressBlend(TUint8 *aAddress, TUint8 aRed, TUint8 aGreen, TUint8 aBlue, TUint8 aAlpha) {
    // NOTE (pent0): Some games gives zero alpha, because alpha blending was not documented before.
    // So if we see a zero alpha, we should only write RGB. This is a hack.
    if (aAlpha == 255 || aAlpha == 0) {
        WriteRgb32ToAddressAlpha(aAddress, aRed, aGreen, aBlue, 255);
        return;
    }

    TUint32 *colorWord = reinterpret_cast<TUint32 *>(aAddress);
    TUint32 color24 = aBlue | (aGreen << 8) | (aRed << 16);

    TUint32 rb = (*colorWord & 0xFF00FF);
    TUint32 g = (*colorWord & 0x00FF00);

    rb += ((color24 & 0xff00ff) - rb) * aAlpha >> 8;
    g += ((color24 & 0x00ff00) - g) * aAlpha >> 8;

    *colorWord &= ~0xFFFFFF;
    *colorWord |= ((rb & 0xff00ff) | (g & 0xff00)) | (0x01000000);
}

static void WriteRgb32ToAddressUNIMPL(TUint8 *aAddress, TUint8 aRed, TUint8 aGreen, TUint8 aBlue, TUint8 aAlpha) {
    // Empty
}

CFbsThirtyTwoBitsDrawDevice::PWriteRgbToAddressFunc CFbsThirtyTwoBitsDrawDevice::GetRgbWriteFunc(CGraphicsContext::TDrawMode aDrawMode) {
    switch (aDrawMode) {
    case CGraphicsContext::EDrawModeWriteAlpha:
        return WriteRgb32ToAddressAlpha;

    case CGraphicsContext::EDrawModeAND:
        return WriteRgb32ToAddressAND;

    case CGraphicsContext::EDrawModeOR:
        return WriteRgb32ToAddressOR;

    case CGraphicsContext::EDrawModeXOR:
        return WriteRgb32ToAddressXOR;

    case CGraphicsContext::EDrawModeNOTSCREEN:
        return WriteRgb32ToAddressNOTSC;

    case CGraphicsContext::EDrawModeANDNOT:
        return WriteRgb32ToAddressANDNOT;

    case CGraphicsContext::EDrawModePEN:
        return (DisplayMode() == EColor16MA) ? WriteRgb32ToAddressBlend : WriteRgb32ToAddressAlpha;

    default:
        Scdv::Log("Unimplemented graphics drawing mode: %d", (TInt)aDrawMode);
        break;
    }

    return WriteRgb32ToAddressUNIMPL;
}

void CFbsThirtyTwoBitsDrawDevice::WriteRgb(TInt aX, TInt aY, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode) {
    TUint8 *pixelStart = GetPixelStartAddress(aX, aY);
    PWriteRgbToAddressFunc writeFunc = GetRgbWriteFunc(aDrawMode);

    writeFunc(pixelStart, (TUint8)aColor.Red(), (TUint8)aColor.Green(), (TUint8)aColor.Blue(), (TUint8)aColor.Alpha());
}

void CFbsThirtyTwoBitsDrawDevice::WriteBinary(TInt aX, TInt aY, TUint32 *aBuffer, TInt aLength, TInt aHeight, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode) {
    TUint8 *pixelAddress = NULL;
    PWriteRgbToAddressFunc writeFunc = GetRgbWriteFunc(aDrawMode);
    TInt increment = GetPixelIncrementUnit() * 4;

    if (aLength > 32 || aX + aLength > LongWidth())
        PanicAtTheEndOfTheWorld();

    const TUint8 red = (TUint8)aColor.Red();
    const TUint8 green = (TUint8)aColor.Green();
    const TUint8 blue = (TUint8)aColor.Blue();
    const TUint8 alpha = (TUint8)aColor.Alpha();

    for (TInt y = aY; y < aY + aHeight; y++) {
        pixelAddress = GetPixelStartAddress(aX, y);

        for (TInt x = aX; x < aX + aLength; x++) {
            if (*aBuffer & (1 << (x - aX))) {
                // Try to reduce if calls pls
                writeFunc(pixelAddress, red, green, blue, alpha);
            }

            pixelAddress += increment;
        }

        aBuffer++;
    }
}

void CFbsThirtyTwoBitsDrawDevice::WriteRgbMulti(TInt aX, TInt aY, TInt aLength, TInt aHeight, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode) {
    TUint8 *pixelAddress = NULL;
    PWriteRgbToAddressFunc writeFunc = GetRgbWriteFunc(aDrawMode);
    TInt increment = GetPixelIncrementUnit() * 4;

    if (aX + aLength > LongWidth())
        PanicAtTheEndOfTheWorld();

    const TUint8 red = (TUint8)aColor.Red();
    const TUint8 green = (TUint8)aColor.Green();
    const TUint8 blue = (TUint8)aColor.Blue();
    const TUint8 alpha = (TUint8)aColor.Alpha();

    for (TInt y = aY; y < aY + aHeight; y++) {
        pixelAddress = GetPixelStartAddress(aX, y);

        for (TInt x = aX; x < aX + aLength; x++) {
            // Try to reduce if calls pls
            writeFunc(pixelAddress, red, green, blue, alpha);
            pixelAddress += increment;
        }
    }
}

void CFbsThirtyTwoBitsDrawDevice::WriteRgbAlphaMulti(TInt aX, TInt aY, TInt aLength, TRgb aColor, const TUint8 *aMaskBuffer) {
    Scdv::Log("Write rgb alpha multi for 16bit mode todo!");
}

void CFbsThirtyTwoBitsDrawDevice::WriteLine(TInt aX, TInt aY, TInt aLength, TUint32 *aBuffer, CGraphicsContext::TDrawMode aDrawMode) {
    TUint8 *pixelAddress = GetPixelStartAddress(aX, aY);
    PWriteRgbToAddressFunc writeFunc = GetRgbWriteFunc(aDrawMode);
    TInt increment = GetPixelIncrementUnit() * 4;

    TUint8 *buffer8 = reinterpret_cast<TUint8 *>(aBuffer);

    if (aX + aLength > LongWidth())
        PanicAtTheEndOfTheWorld();

    for (TInt x = aX; x < aX + aLength; x++) {
        // Try to reduce if calls pls
        writeFunc(pixelAddress, buffer8[0], buffer8[1], buffer8[2], buffer8[3]);

        pixelAddress += increment;
        buffer8 += 4;
    }
}

void CFbsThirtyTwoBitsDrawDevice::SetSize(TSize aSize) {
    iSize = aSize;

    iScanLineWords = iSize.iWidth;
    iLongWidth = iSize.iWidth;
}

TInt CFbsThirtyTwoBitsDrawDevice::ConstructInner(TSize aSize, TInt aDataStride) {
    SetSize(aSize);

    if (aDataStride != -1) {
        if (aDataStride % 4 != 0) {
            return KErrArgument;
        }

        iScanLineWords = aDataStride >> 2;
        iLongWidth = aDataStride >> 2;
    }

    TInt scanLineBufferSize = Max(iSize.iHeight, iSize.iWidth) * 4;
    iScanLineBuffer = User::Alloc(scanLineBufferSize);

    if (iScanLineBuffer == NULL) {
        return KErrNoMemory;
    }

    return KErrNone;
}

TInt CFbsTwentyfourBitAlphaDrawDevice::Construct(TSize aSize, TInt aDataStride) {
    const TInt result = ConstructInner(aSize, aDataStride);
    if (result != KErrNone) {
        return result;
    }

    iDisplayMode = EColor16MA;
    return KErrNone;
}

TInt CFbsTwentyfourBitUnsignedByteDrawDevice::Construct(TSize aSize, TInt aDataStride) {
    const TInt result = ConstructInner(aSize, aDataStride);
    if (result != KErrNone) {
        return result;
    }

    iDisplayMode = EColor16MU;
    return KErrNone;
}

TInt CFbsThirtyTwoBitsDrawDevice::Set(TInt aFactorX, TInt aFactorY, TInt aDivisorX, TInt aDivisorY) {
    Scdv::Log("Set called with %d %d, unimplemented", aFactorX, aFactorY);
    return KErrNone;
}

void CFbsThirtyTwoBitsDrawDevice::Get(TInt &aFactorX, TInt &aFactorY, TInt &aDivisorX, TInt &aDivisorY) {
}

TBool CFbsThirtyTwoBitsDrawDevice::IsScalingOff() {
    return ETrue;
}

TInt CFbsThirtyTwoBitsDrawDevice::GetInterface(TInt aInterfaceId, TAny *&aInterface) {
    switch (aInterfaceId) {
    case KScalingInterfaceID:
        aInterface = static_cast<Scdv::MScalingSettings *>(this);
        return KErrNone;

    case KFastBlit2InterfaceID:
        aInterface = static_cast<Scdv::MFastBlitBlock *>(this);
        return KErrNone;

    case KOrientationInterfaceID:
        aInterface = static_cast<Scdv::MOrientation *>(this);
        return KErrNone;

    default:
        break;
    }

    //Scdv::Log("ERR:: Interface not supported %d", aInterfaceId);
    return KErrNotSupported;
}

///////////////////////////////////////////////
//
//	RGBA screen draw device
//
//////////////////////////////////////////////

void CFbsEKA2L1ScreenDevice::Update(const TRegion &aRegion) {
    UpdateScreen(1, iScreenNumber, aRegion.Count(), aRegion.RectangleList());
}

void CFbsEKA2L1ScreenDevice::UpdateRegion(const TRect &aRect) {
    UpdateScreen(1, iScreenNumber, 1, &aRect);
}

TInt CFbsTwentyfourBitAlphaScreenDrawDevice::Construct(TUint32 aScreenNumber, TSize aSize, TInt aDataStride) {
    iScreenNumber = aScreenNumber;
    return CFbsTwentyfourBitAlphaDrawDevice::Construct(aSize, aDataStride);
}

void CFbsTwentyfourBitAlphaScreenDrawDevice::Update() {
    TRect updateRect;
    updateRect.iTl = TPoint(0, 0);
    updateRect.iBr = updateRect.iTl + iSize;

    UpdateScreen(1, iScreenNumber, 1, &updateRect);
}

TInt CFbsTwentyfourBitUnsignedByteScreenDrawDevice::Construct(TUint32 aScreenNumber, TSize aSize, TInt aDataStride) {
    iScreenNumber = aScreenNumber;
    return CFbsTwentyfourBitUnsignedByteDrawDevice::Construct(aSize, aDataStride);
}

void CFbsTwentyfourBitUnsignedByteScreenDrawDevice::Update() {
    TRect updateRect;
    updateRect.iTl = TPoint(0, 0);
    updateRect.iBr = updateRect.iTl + iSize;

    UpdateScreen(1, iScreenNumber, 1, &updateRect);
}