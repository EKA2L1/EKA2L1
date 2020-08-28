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

#ifndef SCDV_BLIT_H_
#define SCDV_BLIT_H_

#include "scdv/draw.h"

#include <e32def.h>
#include <e32std.h>

class CFbsDrawDevice;

namespace Scdv {
    class MFastBlitBlock {
    public:
        virtual TInt WriteBitmapBlock(const TPoint &aDest,
            CFbsDrawDevice *aSrcDrawDevice,
            const TRect &aSrcRect)
            = 0;

        virtual TInt WriteBitmapBlock(const TPoint &aDest,
            const TUint32 *aSrcBase,
            TInt aSrcStride,
            const TSize &aSrcSize,
            const TRect &aSrcRect)
            = 0;

        virtual const TUint32 *Bits() const = 0;
    };

    class MOrientation {
    public:
        virtual CFbsDrawDevice::TOrientation Orientation() = 0;
    };
}

#endif
