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

#ifndef MEDIA_CLIENT_AUDIO_DISPATCH_H_
#define MEDIA_CLIENT_AUDIO_DISPATCH_H_

#include <e32std.h>

#define HLE_DISPATCH_FUNC(ret, name, ARGS...) \
    ret name(const TUint32 func_id, ##ARGS)

#ifndef EKA2
typedef unsigned long long TUint64;
#endif

extern "C" {
    /// AUDIO PLAYER DISPATCH API
    enum TAudioPlayerFlags {
        EAudioPlayerFlag_StartPreparePlayWhenQueue = 1 << 0
    };

    // Create new audio player instance
    HLE_DISPATCH_FUNC(TAny *, EAudioPlayerNewInstance, TUint32 aInitFlags);

    // Notify when a queued audio segment done playing.
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerNotifyAnyDone, TAny *aInstance, TRequestStatus &aStatus);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerCancelNotifyAnyDone, TAny *aInstance);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSupplyUrl, TAny *aInstance, const TUint16 *aUrl, const TUint aUrlLength);

    HLE_DISPATCH_FUNC(TUint64, EAudioPlayerGetCurrentPlayPos, TAny *aInstance);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetCurrentPlayPos, TAny *aInstance, TInt64 aMicroseconds);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerGetCurrentBitRate, TAny *aInstance);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetBalance, TAny *aInstance, TInt aBalance);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerGetBalance, TAny *aInstance);

    // Supply raw audio data to media player queue.
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSupplyData, TAny *aInstance, TDesC8 &aAudioData);

    // Set volume of the play instance.
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetVolume, TAny *aInstance, const TInt aVolume);

    // Get current volume of the play instance.
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerGetVolume, TAny *aInstance);

    // Get the maximum volume this instance allowed.
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerMaxVolume, TAny *aInstance);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerPlay, TAny *aInstance);
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerStop, TAny *aInstance);
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerPause, TAny *aInstance);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetRepeats, TAny *aInstance, const TInt aTimes, TUint64 aSilenceIntervalMicros);
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerDestroy, TAny *aInstance);
    
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerGetDestinationSampleRate, TAny *aInstance);
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetDestinationSampleRate, TAny *aInstance, const TInt aSampleRate);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerGetDestinationChannelCount, TAny *aInstance);
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetDestinationChannelCount, TAny *aInstance, const TInt aChannelCount);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetDestinationEncoding, TAny *aInstance, const TUint32 aEncoding);
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerGetDestinationEncoding, TAny *aInstance, TUint32 &aEncoding);

    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetContainerFormat, TAny *aInstance, const TUint32 aFourCC);
}

#endif
