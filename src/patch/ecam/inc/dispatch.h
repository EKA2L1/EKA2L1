/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#ifndef CAMERA_DISPATCH_H_
#define CAMERA_DISPATCH_H_

#include <e32std.h>

#define HLE_DISPATCH_FUNC(ret, name, ARGS...) \
    ret name(const TUint32 func_id, ##ARGS)

#ifndef EKA2
typedef unsigned long long TUint64;
#endif

class TCameraInfo;

extern "C" {
HLE_DISPATCH_FUNC(TInt, ECamGetNumberOfCameras);
HLE_DISPATCH_FUNC(TAny*, ECamCreate, TInt aIndex, TCameraInfo &aBasisInfo);
HLE_DISPATCH_FUNC(TAny*, ECamDuplicate, TAny *aInstanceToDuplicate, TCameraInfo &aBasisInfo);
HLE_DISPATCH_FUNC(TInt, ECamClaim, TAny *aInstance);
HLE_DISPATCH_FUNC(void, ECamRelease, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, ECamPowerOn, TAny *aInstance);
HLE_DISPATCH_FUNC(void, ECamPowerOff, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, ECamSetParameter, TAny *aInstance, TInt aKey, TInt aValue);
HLE_DISPATCH_FUNC(TInt, ECamQueryStillImageSize, TAny *aInstance, TInt aFormat, TInt aSizeIndex, TSize &aSize);
HLE_DISPATCH_FUNC(TInt, ECamTakeImage, TAny *aInstance, TInt aSizeIndex, TInt aFormat, TRect *aClip, TRequestStatus &aFinishStatus);
HLE_DISPATCH_FUNC(TInt, ECamReceiveImage, TAny *aInstance, TInt *aSize, const TUint8 *aData);
HLE_DISPATCH_FUNC(TInt, ECamCancelTakeImage, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, ECamQueryVideoFrameDimension, TAny *aInstance, TInt aSizeIndex, TInt aFormat, TSize &aSize);
HLE_DISPATCH_FUNC(TInt, ECamQueryVideoFrameRate, TAny *aInstance, TInt aFrameRateIndex, TInt aSizeIndex, TInt aFormat, TInt aExposure, TReal32 *aRate);
HLE_DISPATCH_FUNC(TInt, ECamTakeVideo, TAny *aInstance, TInt aFrameRateIndex, TInt aSizeIndex, TInt aFormat, TInt aFramesPerRecive, TRect *aClip, TRequestStatus &aFinishStatus);
HLE_DISPATCH_FUNC(TInt, ECamReceiveVideoBuffer, TAny *aInstance, TUint8 *aDataBuffer, TInt *aOffsetBuffer, TUint32 *aTotalSize);
HLE_DISPATCH_FUNC(TInt, ECamCancelTakeVideo, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, ECamStartViewfinder, TAny *aInstance, TSize *aSize, TInt aDisplayMode, TRequestStatus *aStatus);
HLE_DISPATCH_FUNC(TInt, ECamNextViewfinderFrame, TAny *aInstance, TRequestStatus *aStatus);
HLE_DISPATCH_FUNC(TInt, ECamStopViewfinder, TAny *aInstance);
}

#endif
