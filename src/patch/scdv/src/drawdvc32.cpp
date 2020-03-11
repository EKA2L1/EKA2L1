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
	return reinterpret_cast<TUint8*>(iBuffer) + (aX * 4 + aY * PhysicalScanLineBytes());
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
	if (aX + aLength >= LongWidth())
		PanicAtTheEndOfTheWorld();

	TUint8 *pixelStart = GetPixelStartAddress(aX, aY);
	TInt increment = GetPixelIncrementUnit() * 3;
	TInt iterator = 0;
	
	while (iterator < aLength) {
		Mem::Copy(reinterpret_cast<TUint8*>(aBuffer) + iterator * 4, pixelStart, 4);
		pixelStart += increment;
	}
}

void CFbsThirtyTwoBitsDrawDevice::WriteRgbToAddress(TUint8 *aAddress, TUint8 aRed, TUint8 aGreen, TUint8 aBlue, TUint8 aAlpha, CGraphicsContext::TDrawMode aDrawMode) {
	switch (aDrawMode) {
		case CGraphicsContext::EDrawModeWriteAlpha:
			aAddress[0] = aBlue;
			aAddress[1] = aGreen;
			aAddress[2] = aRed;
			aAddress[3] = aAlpha;
			break;
			
		case CGraphicsContext::EDrawModeAND:
			aAddress[0] &= aBlue;
			aAddress[1] &= aGreen;
			aAddress[2] &= aRed;
			aAddress[3] &= aAlpha;
			break;

		case CGraphicsContext::EDrawModeOR:
			aAddress[0] |= aBlue;
			aAddress[1] |= aGreen;
			aAddress[2] |= aRed;
			aAddress[3] |= aAlpha;
			break;

		case CGraphicsContext::EDrawModeXOR:
			aAddress[0] ^= aBlue;
			aAddress[1] ^= aGreen;
			aAddress[2] ^= aRed;
			aAddress[3] ^= aAlpha;
			break;
			
		case CGraphicsContext::EDrawModeNOTSCREEN:
			aAddress[0] = ~(aAddress[0]);
			aAddress[1] = ~(aAddress[1]);
			aAddress[2] = ~(aAddress[2]);
			aAddress[3] = ~(aAddress[3]);
			break;

		case CGraphicsContext::EDrawModeANDNOT:
			aAddress[0] = (~aAddress[0]) & aBlue;
			aAddress[1] = (~aAddress[1]) & aGreen;
			aAddress[2] = (~aAddress[2]) & aRed;
			aAddress[3] = (~aAddress[3]) & aAlpha;
			break;
		
		default:
			Scdv::Log("ERR: Unsupported graphics context draw mode %d", aDrawMode);
			break;
	}	
}

void CFbsThirtyTwoBitsDrawDevice::WriteRgbToAddress(TUint8 *aAddress, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode) {
	WriteRgbToAddress(aAddress, (TUint8)aColor.Red(), (TUint8)aColor.Green(), (TUint8)aColor.Blue(), (TUint8)aColor.Alpha(), aDrawMode);
}

void CFbsThirtyTwoBitsDrawDevice::WriteRgb(TInt aX, TInt aY, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode) {
	TUint8 *pixelStart = GetPixelStartAddress(aX, aY);
	WriteRgbToAddress(pixelStart, aColor, aDrawMode);
}

void CFbsThirtyTwoBitsDrawDevice::WriteBinary(TInt aX,TInt aY,TUint32* aBuffer,TInt aLength,TInt aHeight,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode) {
	TUint8 *pixelAddress = NULL;
	TInt increment = GetPixelIncrementUnit() * 4;

	if (aLength > 32 || aX + aLength >= LongWidth())
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

void CFbsThirtyTwoBitsDrawDevice::WriteRgbMulti(TInt aX,TInt aY,TInt aLength,TInt aHeight,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode) {
	TUint8 *pixelAddress = NULL;
	TInt increment = GetPixelIncrementUnit() * 4;

	if (aX + aLength >= LongWidth())
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

void CFbsThirtyTwoBitsDrawDevice::WriteRgbAlphaMulti(TInt aX,TInt aY,TInt aLength,TRgb aColor,const TUint8* aMaskBuffer) {
	Scdv::Log("Write rgb alpha multi for 16bit mode todo!");
}

void CFbsThirtyTwoBitsDrawDevice::WriteLine(TInt aX,TInt aY,TInt aLength,TUint32* aBuffer,CGraphicsContext::TDrawMode aDrawMode) {
	TUint8 *pixelAddress = GetPixelStartAddress(aX, aY);
	TInt increment = GetPixelIncrementUnit() * 4;

	TUint8 *buffer8 = reinterpret_cast<TUint8*>(aBuffer);
	
	if (aX + aLength >= LongWidth())
		PanicAtTheEndOfTheWorld();

	for (TInt x = aX; x < aX + aLength; x++) {
		// Try to reduce if calls pls
		WriteRgbToAddress(pixelAddress, buffer8[0], buffer8[1], buffer8[2], buffer8[3], aDrawMode);

		pixelAddress += increment;
		buffer8 += 4;
	}
}

void CFbsThirtyTwoBitsDrawDevice::SetSize(TSize aSize) {
	iSize = aSize;

	iScanLineWords = iSize.iWidth;
	iLongWidth = iSize.iWidth;
}

TInt CFbsThirtyTwoBitsDrawDevice::Construct(TSize aSize, TInt aDataStride) {
	iDisplayMode = EColor16MA;
	SetSize(aSize);
	
	if (aDataStride == -1) {
		return KErrNone;
	}
	
	if (aDataStride % 4 != 0) {
		return KErrArgument;
	}
	
	iScanLineWords = aDataStride >> 2;
	iLongWidth = aDataStride >> 2;

	TInt scanLineBufferSize = Max(iSize.iHeight, iSize.iWidth) * 4;
	iScanLineBuffer = User::Alloc(scanLineBufferSize);
	
	if (iScanLineBuffer == NULL) {
		return KErrNoMemory;
	}
	
	return KErrNone;
}

///////////////////////////////////////////////
//
//	RGBA screen draw device
//
//////////////////////////////////////////////

void CFbsTwentyfourBitAlphaScreenDrawDevice::Update() {
	TRect updateRect;
	updateRect.iTl = TPoint(0, 0);
	updateRect.iBr = updateRect.iTl + iSize;

	UpdateScreen(1, &updateRect);
}

void CFbsTwentyfourBitAlphaScreenDrawDevice::Update(const TRegion& aRegion) {
	UpdateScreen(aRegion.Count(), aRegion.RectangleList());
}

void CFbsTwentyfourBitAlphaScreenDrawDevice::UpdateRegion(const TRect& aRect) {
	UpdateScreen(1, &aRect);
}
