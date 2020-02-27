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

#ifndef SCDV_DRAW_H_
#define SCDV_DRAW_H_

#include <e32std.h>
#include <gdi.h>
#include <w32std.h>

struct TScreenInfo {
    TBool iWindowHandleValid;
    TAny *iWindowHandle;
    TBool iScreenAddressValid;
    TAny *iScreenAddress;
    TSize iScreenSize;
};

class CFbsDrawDevice : public CBase {
public:
    enum TShadowMode {
        ENoShadow = 0x0,
        EShadow = 0x1,
        EFade = 0x2,
        EShadowFade = 0x3
    };
 
    enum TOrientation {
        EOrientationNormal,
        EOrientationRotate90,
        EOrientationRotate180,
        EOrientationRotate270
    };

public:
    IMPORT_C static CFbsDrawDevice* NewScreenDeviceL(TScreenInfo aInfo, TDisplayMode aDispMode);
    IMPORT_C static CFbsDrawDevice* NewScreenDeviceL(TInt aScreenNo, TDisplayMode aDispMode);
    IMPORT_C static CFbsDrawDevice* NewBitmapDeviceL(TScreenInfo aInfo, TDisplayMode aDispMode, TInt aDataStride);
    IMPORT_C static CFbsDrawDevice* NewBitmapDeviceL(const TSize& aSize, TDisplayMode aDispMode, TInt aDataStride);
    IMPORT_C static TDisplayMode DisplayMode16M();

    virtual TDisplayMode DisplayMode() const = 0;
    virtual TInt LongWidth() const = 0;
    virtual void MapColors(const TRect& aRect,const TRgb* aColors,TInt aNumPairs,TBool aMapForwards) = 0;
    virtual void ReadLine(TInt aX,TInt aY,TInt aLength,TAny* aBuffer,TDisplayMode aDispMode) const = 0;
    virtual TRgb ReadPixel(TInt aX,TInt aY) const = 0;
    virtual TUint32* ScanLineBuffer() const = 0;
    
    /**
     * \brief       Get byte width of the device.
     * \returns     Byte width.
     */
    virtual TInt ScanLineBytes() const = 0;
    virtual TDisplayMode ScanLineDisplayMode() const = 0;
    virtual TSize SizeInPixels() const = 0;
    virtual TInt HorzTwipsPerThousandPixels() const = 0;
    virtual TInt VertTwipsPerThousandPixels() const = 0;
    virtual void OrientationsAvailable(TBool aOrientation[4]) = 0;
    virtual TBool SetOrientation(TOrientation aOrientation) = 0;
    virtual void WriteBinary(TInt aX,TInt aY,TUint32* aBuffer,TInt aLength,TInt aHeight,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode) = 0;
    virtual void WriteBinaryLine(TInt aX,TInt aY,TUint32* aBuffer,TInt aLength,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode) = 0;
    virtual void WriteBinaryLineVertical(TInt aX,TInt aY,TUint32* aBuffer,TInt aHeight,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode,TBool aUp) = 0;
    virtual void WriteRgb(TInt aX,TInt aY,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode) = 0;
    virtual void WriteRgbMulti(TInt aX,TInt aY,TInt aLength,TInt aHeight,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode) = 0;
    virtual void WriteRgbAlphaLine(TInt aX,TInt aY,TInt aLength,TUint8* aRgbBuffer,TUint8* aMaskBuffer, CGraphicsContext::TDrawMode aDrawMode) = 0;
    virtual void WriteLine(TInt aX,TInt aY,TInt aLength,TUint32* aBuffer,CGraphicsContext::TDrawMode aDrawMode) = 0;

public:
    virtual TInt InitScreen() = 0;
    virtual void SetAutoUpdate(TBool aValue) = 0;
    virtual void SetBits(TAny* aBits) = 0;
    virtual TInt SetCustomPalette(const CPalette*) = 0;
    virtual TInt GetCustomPalette(CPalette*&) = 0;
    virtual void SetDisplayMode(CFbsDrawDevice* aDrawDevice) = 0;
    virtual void SetDitherOrigin(const TPoint&) = 0;
    virtual void SetUserDisplayMode(TDisplayMode) = 0;
    virtual void SetShadowMode(TShadowMode) = 0;
    virtual void SetFadingParameters(TUint8 /*aBlackMap*/,TUint8 /*aWhiteMap*/) = 0;
    virtual void ShadowArea(const TRect&) = 0;
    virtual void ShadowBuffer(TInt,TUint32*) = 0;
    virtual void Update() = 0;
    virtual void Update(const TRegion&) = 0;
    virtual void UpdateRegion(const TRect&) = 0;

public:
    virtual void WriteRgbAlphaMulti(TInt aX,TInt aY,TInt aLength,TRgb aColor,const TUint8* aMaskBuffer) = 0;
    virtual void WriteRgbAlphaLine(TInt aX,TInt aY,TInt aLength,
                                   const TUint8* aRgbBuffer1,
                                   const TUint8* aBuffer2,
                                   const TUint8* aMaskBuffer,
                                   CGraphicsContext::TDrawMode aDrawMode) = 0;
    virtual TInt GetInterface(TInt aInterfaceId, TAny*& aInterface) = 0;
    virtual void GetDrawRect(TRect& aDrawRect) const = 0;
    virtual void SwapWidthAndHeight() = 0;
};

#endif
