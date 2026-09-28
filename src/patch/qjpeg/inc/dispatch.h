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

#ifndef QJPEG_DISPATCH_H_
#define QJPEG_DISPATCH_H_

#include <e32std.h>

// The first argument is overwritten by the dispatch stub with the function
// number, so callers pass any value for it.
#define HLE_DISPATCH_FUNC(ret, name, ARGS...) \
    ret name(const TUint32 func_id, ##ARGS)

// Byte order of the destination buffer, low address first.
enum TImageDecodePixelFormat {
    EImageDecodePixelFormatBgra8888 = 0,        // QImage::Format_RGB32 / Format_ARGB32
    EImageDecodePixelFormatRgba8888 = 1,
    EImageDecodePixelFormatRgb888 = 2
};

struct TImageDecodeInfo {
    TUint32 iWidth;
    TUint32 iHeight;
    TUint32 iComponents;
};

extern "C" {
HLE_DISPATCH_FUNC(TInt, EImageDecodeInfo, const TAny *aData, TUint32 aSize, TImageDecodeInfo *aInfo);
HLE_DISPATCH_FUNC(TInt, EImageDecode, const TAny *aData, TUint32 aSize, TAny *aDest, TUint32 aDestSize,
    TUint32 aDestStride, TUint32 aDestFormat);
}

#endif // QJPEG_DISPATCH_H_
