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

#ifndef SCDV_DVC_ALGO_H_
#define SCDV_DVC_ALGO_H_

#include <scdv/blit.h>
#include <scdv/draw.h>
#include <e32std.h>

void PanicAtTheEndOfTheWorld();

class CFbsDrawDeviceAlgorithm: public CFbsDrawDevice,
                                public Scdv::MOrientation {
protected:
	TInt iScanLineWords;
	TInt iLongWidth;
    TOrientation iOrientation;
	
public:
	explicit CFbsDrawDeviceAlgorithm();

	virtual TInt LongWidth() const;
	virtual TInt ScanLineBytes() const;
	virtual TInt HorzTwipsPerThousandPixels() const;
	virtual TInt VertTwipsPerThousandPixels() const;
	virtual void OrientationsAvailable(TBool aOrientation[4]);
	virtual void ReadLine(TInt aX,TInt aY,TInt aLength,TAny* aBuffer,TDisplayMode aDispMode) const;
    virtual void WriteBinaryLine(TInt aX,TInt aY,TUint32* aBuffer,TInt aLength,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode);
    
    // NOTE TODO
    virtual void WriteBinaryLineVertical(TInt aX,TInt aY,TUint32* aBuffer,TInt aHeight,TRgb aColor,CGraphicsContext::TDrawMode aDrawMode,TBool aUp);
    virtual void WriteRgbAlphaLine(TInt aX,TInt aY,TInt aLength,TUint8* aRgbBuffer,TUint8* aMaskBuffer, CGraphicsContext::TDrawMode aDrawMode);
    virtual void WriteRgbAlphaMulti(TInt aX,TInt aY,TInt aLength,TRgb aColor,const TUint8* aMaskBuffer);
    virtual void WriteRgbAlphaLine(TInt aX,TInt aY,TInt aLength,
                                       const TUint8* aRgbBuffer1,
                                       const TUint8* aBuffer2,
                                       const TUint8* aMaskBuffer,
                                       CGraphicsContext::TDrawMode aDrawMode);
    virtual TInt GetInterface(TInt aInterfaceId, TAny*& aInterface);
    virtual void SwapWidthAndHeight();
    // END TODO
   
	virtual void MapColors(const TRect& aRect,const TRgb* aColors,TInt aNumPairs,TBool aMapForwards);

    TInt GetPixelIncrementUnit() const;
    TInt PhysicalScanLineBytes() const;
    
    void TransformCoordinateToPhysical(TInt aX, TInt aY, TInt &aNewX, TInt &aNewY) const;
    
    virtual void ReadLineRaw(TInt aX, TInt aY, TInt aLength, TAny *aBuffer) const = 0;

    virtual TOrientation Orientation() {
    	return iOrientation;
    }
};

#endif
