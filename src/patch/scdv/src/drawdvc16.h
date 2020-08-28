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

#ifndef SCDV_DVC_SIXTEEN_BITS_H_
#define SCDV_DVC_SIXTEEN_BITS_H_

#include "drawdvcb.h"

class CFbsSixteenBitDrawDevice : public CFbsDrawDeviceByteBuffer {
public:
    TInt Construct(TSize aSize, TInt aDataStride);
    void SetSize(TSize aSize);

    virtual void WriteRgbToAddress(TUint8 *aAddress, TRgb aColor, CGraphicsContext::TDrawMode aDrawMode);
    virtual void WriteRgbToAddress(TUint8 *aAddress, TUint8 aRed, TUint8 aGreen, TUint8 aBlue, CGraphicsContext::TDrawMode aDrawMode);

    virtual TRgb ReadPixel(TInt aX, TInt aY) const;
};

#endif
