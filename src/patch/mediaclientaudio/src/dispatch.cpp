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

#include <dispatch.h>

HLE_DISPATCH_FUNC(TAny *, EAudioPlayerNewInstance, void *){
    CALL_HLE_DISPATCH(0x20)
}

// Notify when a queued audio segment done playing.
HLE_DISPATCH_FUNC(TInt, EAudioPlayerNotifyAnyDone, TAny *aInstance, TRequestStatus &aStatus) {
    (void)aInstance;
    (void)aStatus;

    CALL_HLE_DISPATCH(0x21)
}

// Supply a media file to the media player queue.
HLE_DISPATCH_FUNC(TInt, EAudioPlayerSupplyUrl, TAny *aInstance, const TUint16 *aUrl, const TUint aUrlLength) {
    (void)aInstance;
    (void)aUrl;
    (void)aUrlLength;

    CALL_HLE_DISPATCH(0x22);
}

// Supply raw audio data to media player queue.
HLE_DISPATCH_FUNC(TInt, EAudioPlayerSupplyData, TAny *aInstance, const TDesC &aAudioData, const TUint32 aEncoding, const TUint32 aFreq,
    const TUint32 aChannels) {
    (void)aInstance;
    (void)aAudioData;
    (void)aEncoding;
    (void)aFreq;
    (void)aChannels;

    CALL_HLE_DISPATCH(0x23)
}

// Set volume of the play instance.
HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetVolume, TAny *aInstance, const TInt aVolume) {
    (void)aInstance;
    (void)aVolume;

    CALL_HLE_DISPATCH(0x24)
}

// Get current volume of the play instance.
HLE_DISPATCH_FUNC(TInt, EAudioPlayerGetVolume, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x25)
}

// Get the maximum volume this instance allowed.
HLE_DISPATCH_FUNC(TInt, EAudioPlayerMaxVolume, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x26)
}

HLE_DISPATCH_FUNC(TInt, EAudioPlayerPlay, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x27)
}
HLE_DISPATCH_FUNC(TInt, EAudioPlayerStop, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x28)
}

HLE_DISPATCH_FUNC(TInt, EAudioPlayerPause, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x29)
}

HLE_DISPATCH_FUNC(TInt, EAudioPlayerCancelNotifyAnyDone, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x2A)
}

HLE_DISPATCH_FUNC(TUint64, EAudioPlayerGetCurrentPlayPos, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x2B)
}

HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetCurrentPlayPos, TAny *aInstance, TInt64 aMicroseconds) {
    (void)aInstance;
    (void)aMicroseconds;

    CALL_HLE_DISPATCH(0x2C)
}

HLE_DISPATCH_FUNC(TInt, EAudioPlayerGetCurrentBitRate, TAny *aInstance) {
    (void)aInstance;

    CALL_HLE_DISPATCH(0x2D)
}

HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetBalance, TAny *aInstance, TInt aBalance) {
    (void)aInstance;
    (void)aBalance;

    CALL_HLE_DISPATCH(0x2E);
}

HLE_DISPATCH_FUNC(TInt, EAudioPlayerGetBalance, TAny *aInstance) {
    (void)aInstance;

    CALL_HLE_DISPATCH(0x2F);
}

HLE_DISPATCH_FUNC(TInt, EAudioPlayerSetRepeats, TAny *aInstance, const TInt aTimes, TUint64 aSilenceIntervalMicros) {
    (void)aInstance;
    (void)aTimes;
    (void)aSilenceIntervalMicros;

    CALL_HLE_DISPATCH(0x30);
}

HLE_DISPATCH_FUNC(TInt, EAudioPlayerDestroy, TAny *aInstance) {
    (void)aInstance;

    CALL_HLE_DISPATCH(0x31);
}
