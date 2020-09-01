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

#ifndef SCDV_DVC_TWELVE_BITS_H_
#define SCDV_DVC_TWELVE_BITS_H_

#include "drawdvcbuf.h"

class CFbsDrawDeviceByteBuffer : public CFbsDrawDeviceBuffer {
protected:
    TInt iByteCount;

public:
    virtual TUint8 *GetPixelStartAddress(TInt aX, TInt aY) const;
    virtual void ReadLineRaw(TInt aX, TInt aY, TInt aLength, TAny *aBuffer) const;
    virtual void WriteBinary(TInt aX, TInt aY, TUint32 *aBuffer, TInt aLength, TInt aHeight, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode);
    virtual void WriteLine(TInt aX, TInt aY, TInt aLength, TUint32 *aBuffer, CGraphicsContext::TDrawMode aDrawMode);

    virtual void WriteRgbToAddress(TUint8 *aAddress, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode) = 0;
    virtual void WriteRgbToAddress(TUint8 *aAddress, TUint8 *aRawColor, CGraphicsContext::TDrawMode aDrawMode) = 0;

    virtual void WriteRgb(TInt aX, TInt aY, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode);
    virtual void WriteRgbMulti(TInt aX, TInt aY, TInt aLength, TInt aHeight, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode);
    virtual void WriteRgbAlphaMulti(TInt aX, TInt aY, TInt aLength, TRgb aColor, const TUint8 *aMaskBuffer);
};

#endif
