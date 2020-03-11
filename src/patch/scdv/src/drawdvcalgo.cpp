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

#include "drawdvcalgo.h"
#include <scdv/log.h>
#include <scdv/panic.h>

/**
 * \brief Get number of bits per pixel of each display mode.
 */
static TInt GetBppFromDisplayMode(TDisplayMode aMode) {
	switch (aMode) {
		case EGray2:
			return 1;
	
		case EGray4:
			return 2;
			
		case EGray16:
		case EColor16:
			return 4;
			
		case EGray256:
		case EColor256:
			return 8;
		
		case EColor4K:
			return 12;
		
		case EColor64K:
			return 16;
		
		case EColor16M:
			return 24;
			
		case EColor16MA:
		case EColor16MAP:
		case EColor16MU:
			return 32;
		
		default:
			break;
	}
	
	return 32;
}

CFbsDrawDeviceAlgorithm::CFbsDrawDeviceAlgorithm()
	: iScanLineWords(0)
	, iLongWidth(0) {
	
}

void CFbsDrawDeviceAlgorithm::MapColors(const TRect& aRect,const TRgb* aColors,TInt aNumPairs,TBool aMapForwards) {
	// Iterate through pixels. Order doesnt matter.
	for (TInt x = aRect.iTl.iX; x <= aRect.iBr.iX; x++) {
		for (TInt y = aRect.iTl.iY; y <= aRect.iBr.iY; y++) {
			const TRgb pixel = ReadPixel(x, y);
			const TInt targetToSwapIndex = static_cast<TInt>(aMapForwards);
			
			for (TInt pairIte = 0; pairIte < aNumPairs; pairIte++) {
				// Iterate through pairs to find match one. Then swap them by rewriting.
				if (aColors[(pairIte << 1) + targetToSwapIndex] == pixel) {
					WriteRgb(x, y, aColors[(pairIte << 1) + (1 - targetToSwapIndex)], CGraphicsContext::EDrawModeAND);
				}
			}
		}
	}
}

/**
 * \brief Query how many bytes a line of bitmap data consumes.
 */
TInt CFbsDrawDeviceAlgorithm::ScanLineBytes() const {
	const TInt scanLineBytes = iScanLineWords << 2;
	
	if (GetOrientation() & 1) {
		const TSize size = SizeInPixels();
		return (size.iWidth == 0) ? 0 : scanLineBytes * size.iHeight / size.iWidth;
	}
	
	return scanLineBytes;
}

TInt CFbsDrawDeviceAlgorithm::PhysicalScanLineBytes() const {
	return iScanLineWords << 2;
}

TInt CFbsDrawDeviceAlgorithm::LongWidth() const {
	if (GetOrientation() & 1) {
		const TSize size = SizeInPixels();
		return (size.iWidth == 0) ? 0 : iLongWidth * size.iHeight / size.iWidth;
	}
	
	return iLongWidth;
}

TInt CFbsDrawDeviceAlgorithm::HorzTwipsPerThousandPixels() const {
	return 0;
}

TInt CFbsDrawDeviceAlgorithm::VertTwipsPerThousandPixels() const {
	return 0;
}

void CFbsDrawDeviceAlgorithm::OrientationsAvailable(TBool aOrientation[4]) {
	aOrientation[EOrientationNormal] = ETrue;
	aOrientation[EOrientationRotate90] = ETrue;
	aOrientation[EOrientationRotate180] = ETrue;
	aOrientation[EOrientationRotate270] = ETrue;
}


TInt CFbsDrawDeviceAlgorithm::GetPixelIncrementUnit() const {
	switch (GetOrientation()) {
		case EOrientationNormal:
			return 1;
		
		case EOrientationRotate180:
			return -1;
			
		case EOrientationRotate90:
			return -LongWidth();
			
		case EOrientationRotate270:
			return LongWidth();
			
		default:
			break;
	}
	
	return 1;
}

void CFbsDrawDeviceAlgorithm::TransformCoordinateToPhysical(TInt aX, TInt aY, TInt &aNewX, TInt &aNewY) const {
	switch (GetOrientation()) {
		case EOrientationRotate90:
			aNewY = SizeInPixels().iHeight - aX;
			aNewX = aY;
			break;
			
		case EOrientationRotate180:
			aNewX = SizeInPixels().iWidth - aX;
			aNewY = SizeInPixels().iHeight - aY;
			break;
			
		case EOrientationRotate270:
			aY = aX;
			aX = SizeInPixels().iWidth - aY;
			break;
			
		default:
			break;
	}
}

void PanicAtTheEndOfTheWorld() {
	Scdv::Log("Trying to read a line but out of bounds");
	Scdv::Panic(Scdv::EPanicOutOfBounds);
}

void CFbsDrawDeviceAlgorithm::ReadLine(TInt aX,TInt aY,TInt aLength,TAny* aBuffer,TDisplayMode aDispMode) const {
	if (DisplayMode() == aDispMode) {
		// Mode is the same, so we can write with no modification
		ReadLineRaw(aX, aY, aLength, aBuffer);
		return;
	}
	
	// Read each pixel one by one oh my god...
	if (aX + aLength >= LongWidth()) {
		PanicAtTheEndOfTheWorld();
		return;
	}
	
	TInt ite = 0;
	TUint32 *bufferWord = reinterpret_cast<TUint32*>(aBuffer);
	TUint8 *bufferByte = reinterpret_cast<TUint8*>(aBuffer);
	
	while (ite < aLength) {
		TRgb color = ReadPixel(aX + ite, aY);
		TInt colorTransformed = 0;
		
		switch (aDispMode) {
			case EGray2:
				colorTransformed = color.Gray2();
				break;
				
			case EGray4:
				colorTransformed = color.Gray4();
				break;
				
			case EGray16:
				colorTransformed = color.Gray16();
				break;
				
			case EGray256:
				colorTransformed = color.Gray256();
				break;
				
			case EColor16:
				colorTransformed = color.Color16();
				break;
				
			case EColor256:
				colorTransformed = color.Color256();
				break;
				
			case EColor64K:
				colorTransformed = color.Color64K();
				break;
				
			case EColor16M:
				colorTransformed = color.Color16M();
				break;
				
			case EColor4K:
				colorTransformed = color.Color4K();
				break;
				
			case EColor16MU:
				colorTransformed = color.Color16MU();
				break;
				
			case EColor16MAP:
				colorTransformed = color.Color16MAP();
				break;
				
			case EColor16MA:
				colorTransformed = color.Color16MA();
				break;
				
			default:
				Scdv::Log("Unsupported color format to transform %d", aDispMode);
				return;
		}
		
		switch (aDispMode) {
			case EGray2:
				if (ite == 32)		// The word is full
					bufferWord++;
				
				*bufferWord |= (colorTransformed & 1) << (ite & 31);
				
				break;
			
			case EGray4:
				if (ite == 16)		// This word is full too
					bufferWord++;
				
				*bufferWord |= (colorTransformed & 3) << ((ite & 15) << 1);
				break;
				
			case EGray16:
			case EColor16:
				if (ite == 8)		// OMG this word is also full
					bufferWord++;
				
				*bufferWord |= (colorTransformed & 15) << ((ite & 7) << 2);
				break;
				
			case EGray256:
			case EColor256:
				if (ite == 4)
					bufferWord++;
				
				*bufferWord |= (colorTransformed & 0xFF) << ((ite & 3) << 3);
				break;
			
			case EColor64K:
				if (ite == 2)
					bufferWord++;
				
				*bufferWord |= (colorTransformed & 0xFFFF) << ((ite & 1) << 4);
				break;
				
			case EColor16M:
				bufferByte[ite++] = colorTransformed & 0xF;
				bufferByte[ite++] = (colorTransformed & 0xF0) >> 8;
				bufferByte[ite++] = (colorTransformed & 0xF00) >> 16;
				break;
				
			case EColor16MA:
			case EColor16MAP:
			case EColor16MU:
				*bufferWord++ = colorTransformed;
				break;
				
			case EColor4K:
				Scdv::Log("Trying to get 12bpp color line, warning unstable!");

				if (ite & 1) {
					bufferByte[ite++] |= (colorTransformed & 0xF) << 4;
					bufferByte[ite++] = ((colorTransformed >> 4) & 0xFF);
				} else {
					bufferByte[ite++] = (colorTransformed & 0xFF);
					bufferByte[ite] |= ((colorTransformed >> 8) & 0xF);
				}
				
				break;
				
			default:
				break;
		}
	}
}

void CFbsDrawDeviceAlgorithm::WriteBinaryLine(TInt aX,TInt aY,TUint32* aBuffer,TInt aLength,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode) {
	while (aLength > 32) {
		WriteBinary(aX, aY, aBuffer, 32, 1, aColor, aDrawMode);
		aLength -= 32;
		aBuffer++;
		aY++;
	}
	
	WriteBinary(aX, aY, aBuffer, aLength, 1, aColor, aDrawMode);
}

// TODO
void CFbsDrawDeviceAlgorithm::WriteBinaryLineVertical(TInt aX,TInt aY,TUint32* aBuffer,TInt aHeight,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode,TBool aUp) {
	Scdv::Log("ERR: Write binary line vertical is not supported! TODO");
}

void CFbsDrawDeviceAlgorithm::WriteRgbAlphaLine(TInt aX,TInt aY,TInt aLength,TUint8* aRgbBuffer,TUint8* aMaskBuffer, CGraphicsContext::TDrawMode aDrawMode) {
	Scdv::Log("ERR: Write RGB alpha line not supported! TODO!");
}
void CFbsDrawDeviceAlgorithm::WriteRgbAlphaMulti(TInt aX,TInt aY,TInt aLength,TRgb aColor,const TUint8* aMaskBuffer) {
	Scdv::Log("ERR: Write RGB alpha multi not supported! TODO!");
}

void CFbsDrawDeviceAlgorithm::WriteRgbAlphaLine(TInt aX,TInt aY,TInt aLength,
                                   const TUint8* aRgbBuffer1,
                                   const TUint8* aBuffer2,
                                   const TUint8* aMaskBuffer,
                                   CGraphicsContext::TDrawMode aDrawMode) {
	Scdv::Log("ERR: Write RGB alpha line extended not supported! TODO!");
}

TInt CFbsDrawDeviceAlgorithm::GetInterface(TInt aInterfaceId, TAny*& aInterface) {
	Scdv::Log("ERR: Get interface not supported! TODO!");
	return KErrNotSupported;
}

void CFbsDrawDeviceAlgorithm::SwapWidthAndHeight() {
	Scdv::Log("Swap width and height not supported! TODO!");
}
