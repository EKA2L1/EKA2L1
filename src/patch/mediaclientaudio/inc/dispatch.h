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

#define HLE_DISPATCH_FUNC(ret, name, ...) \
    ret name(const TUint32 func_id, __VA_ARGS__)

extern "C" {
    /// AUDIO PLAYER DISPATCH API

    // Create new audio player instance
    HLE_DISPATCH_FUNC(TAny *, EAudioPlayerNewInstance, void *);

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
    HLE_DISPATCH_FUNC(TInt, EAudioPlayerSupplyData, TAny *aInstance, const TDesC &aAudioData, const TUint32 aEncoding, const TUint32 aFreq,
        const TUint32 aChannels);

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
}

#endif
