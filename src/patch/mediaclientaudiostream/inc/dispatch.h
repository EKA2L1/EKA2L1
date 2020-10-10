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

#ifndef MEDIA_CLIENT_AUDIO_STREAM_DISPATCH_H_
#define MEDIA_CLIENT_AUDIO_STREAM_DISPATCH_H_

#include <e32std.h>

#define HLE_DISPATCH_FUNC(ret, name, ARGS...) \
    ret name(const TUint32 func_id, ##ARGS)

#ifndef EKA2
typedef unsigned long long TUint64;
#endif

extern "C" {
    // DSP functions
    HLE_DISPATCH_FUNC(TAny *, EAudioDspOutStreamCreate, void *aNot);
    HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamWrite, TAny *aInstance, const TUint8 *aData, const TUint32 aSize);

    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamSetProperties, TAny *aInstance, TInt aFrequency, TInt aChannels);
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamStart, TAny *aInstance);
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamStop, TAny *aInstance);

    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamGetSupportedFormats, TUint32 *aFourCCs, TUint32 &aListCount);
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamSetFormat, TAny *aInstance, const TUint32 aFourCC);
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamGetFormat, TAny *aInstance, TUint32 &aFourCC);

    HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamSetVolume, TAny *aInstance, TInt aVolume);
    HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamMaxVolume, TAny *aInstance);
    HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamGetVolume, TAny *aInstance);

    // Notify that a frame has been wrote to the audio driver's buffer on HLE side.
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamNotifyBufferSentToDriver, TAny *aInstance, TRequestStatus &aStatus);
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamNotifyEnd, TAny *aInstance, TRequestStatus &aStatus);
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamCancelNotifyBufferSentToDriver, TAny *aInstance);

    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamDestroy, TAny *aInstance);

    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamSetBalance, TAny *aInstance, TInt aBalance);
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamGetBalance, TAny *aInstance);
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamBytesRendered, TAny *aInstance, TUint64 &aBytesRendered);
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamPosition, TAny *aInstance, TUint64 &aPosition);
    HLE_DISPATCH_FUNC(TInt, EAudioDspStreamResetStat, TAny *aInstance);
}

#endif
