/*
 * Copyright (c) 2022 EKA2L1 Team.
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

#ifndef MEDIACLIENTVIDEO_DISPATCH_H_
#define MEDIACLIENTVIDEO_DISPATCH_H_

#include <e32std.h>

#define HLE_DISPATCH_FUNC(ret, name, ARGS...) \
    ret name(const TUint32 func_id, ##ARGS)

extern "C" {
HLE_DISPATCH_FUNC(TAny*, EVideoPlayerCreate);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerRegisterWindow, TAny *aInstance, TInt aWsSsHandle, TInt aWindowHandle);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerSetClipRect, TAny *aInstance, TInt aWindowManagedHandle, const TRect *aRect);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerOpenUrl, TAny *aInstance, const TDesC *aUrl);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerOpenDes, TAny *aInstance, const TDesC8 *aDescContent);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerPlay, TAny *aInstance, const TTimeIntervalMicroSeconds *aRange);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerPause, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerStop, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerDestroy, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerGetVideoFps, TAny *aInstance, TReal32 *aFps);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerSetVideoFps, TAny *aInstance, const TReal32 *aFps);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerGetBitrate, TAny *aInstance, TBool aIsAudio);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerSetPlayDoneNotification, TAny *aInstance, TRequestStatus *aStatus);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerCancelPlayDoneNotification, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerClose, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerSetPosition, TAny *aInstance, const TTimeIntervalMicroSeconds *aPosition);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerGetPosition, TAny *aInstance, const TTimeIntervalMicroSeconds *aPosition);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerGetDuration, TAny *aInstance, const TTimeIntervalMicroSeconds *aDuration);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerMaxVolume, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerCurrentVolume, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerSetVolume, TAny *aInstance, TInt aNewVolume);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerSetRotation, TAny *aInstance, TInt aRotation);
HLE_DISPATCH_FUNC(TInt, EVideoPlayerUnregisterWindow, TAny *aInstance, TInt aManagedWindowHandle);
}

#endif
