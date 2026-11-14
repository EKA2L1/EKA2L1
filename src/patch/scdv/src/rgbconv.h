/*
 * Copyright (c) 2026 EKA2L1 Team.
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

#ifndef SCDV_RGBCONV_H_
#define SCDV_RGBCONV_H_

#include <gdi.h>

// TRgb display-mode conversions, kept local because gdi.dll on Symbian 9.1 does not export them.
// They follow the gdi definitions and use only the component accessors, so any TRgb layout works.
namespace Scdv {
    inline TInt Gray256(const TRgb aColor) {
        return ((aColor.Red() << 1) + aColor.Green() + (aColor.Green() << 2) + aColor.Blue()) >> 3;
    }

    inline TInt Gray2(const TRgb aColor) {
        return Gray256(aColor) >> 7;
    }

    inline TInt Gray4(const TRgb aColor) {
        return Gray256(aColor) >> 6;
    }

    inline TInt Gray16(const TRgb aColor) {
        return Gray256(aColor) >> 4;
    }

    inline TInt Color4K(const TRgb aColor) {
        return ((aColor.Red() & 0xF0) << 4) | (aColor.Green() & 0xF0) | (aColor.Blue() >> 4);
    }

    inline TInt Color64K(const TRgb aColor) {
        return ((aColor.Red() & 0xF8) << 8) | ((aColor.Green() & 0xFC) << 3) | (aColor.Blue() >> 3);
    }

    inline TInt Color16M(const TRgb aColor) {
        return (aColor.Red() << 16) | (aColor.Green() << 8) | aColor.Blue();
    }

    inline TRgb Color4KToRgb(const TInt aColor4K) {
        return TRgb(((aColor4K >> 8) & 0xF) * 0x11, ((aColor4K >> 4) & 0xF) * 0x11, (aColor4K & 0xF) * 0x11);
    }

    inline TRgb Color64KToRgb(const TInt aColor64K) {
        TInt red = (aColor64K & 0xF800) >> 8;
        TInt green = (aColor64K & 0x07E0) >> 3;
        TInt blue = (aColor64K & 0x001F) << 3;

        return TRgb(red + (red >> 5), green + (green >> 6), blue + (blue >> 5));
    }

#ifdef EKA2
    inline TUint Color16MA(const TRgb aColor) {
        return (static_cast<TUint>(aColor.Alpha()) << 24) | static_cast<TUint>(Color16M(aColor));
    }

    inline TUint Color16MAP(const TRgb aColor) {
        const TUint32 pixel = Color16MA(aColor);
        const TUint32 alpha = pixel >> 24;

        if (alpha == 0) {
            return 0;
        }

        if (alpha == 0xFF) {
            return pixel;
        }

        const TUint32 scaledRB = (pixel & 0x00FF00FF) * (alpha + 1);
        const TUint32 scaledG = (pixel & 0x0000FF00) * (alpha + 1);

        return (pixel & 0xFF000000) | ((scaledRB >> 8) & 0x00FF00FF) | ((scaledG >> 8) & 0x0000FF00);
    }
#endif
}

#endif
