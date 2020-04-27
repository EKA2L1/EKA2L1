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
#include <log.h>

HLE_DISPATCH_FUNC(TAny *, EAudioDspOutStreamCreate, void *aNot) {
    (void)aNot;
    CALL_HLE_DISPATCH(0x40);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamWrite, TAny *aInstance, const TUint8 *aData, const TUint32 aSize) {
    (void)aInstance;
    (void)aData;
    (void)aSize;

    CALL_HLE_DISPATCH(0x41);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamSetProperties, TAny *aInstance, TInt aFrequency, TInt aChannels) {
    (void)aInstance;
    (void)aFrequency;
    (void)aChannels;

    CALL_HLE_DISPATCH(0x42);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamStart, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x43);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamStop, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x44);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamGetSupportedFormats, TUint32 *aFourCCs, TUint32 &aListCount) {
    (void)aFourCCs;
    (void)aListCount;

    CALL_HLE_DISPATCH(0x45);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamSetFormat, TAny *aInstance, const TUint32 aFourCC) {
    (void)aInstance;
    (void)aFourCC;

    CALL_HLE_DISPATCH(0x46);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamSetVolume, TAny *aInstance, TInt aVolume) {
    (void)aInstance;
    (void)aVolume;

    CALL_HLE_DISPATCH(0x47);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamMaxVolume, TAny *aInstance) {
    (void)aInstance;

    CALL_HLE_DISPATCH(0x48);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamNotifyBufferSentToDriver, TAny *aInstance, TRequestStatus &aStatus) {
    (void)aInstance;
    (void)aStatus;

    CALL_HLE_DISPATCH(0x49);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamNotifyEnd, TAny *aInstance, TRequestStatus &aStatus) {
    (void)aInstance;
    (void)aStatus;

    CALL_HLE_DISPATCH(0x4A);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamDestroy, TAny *aInstance) {
    (void)aInstance;

    CALL_HLE_DISPATCH(0x4B);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamGetVolume, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x4C);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamSetBalance, TAny *aInstance, TInt aBalance) {
    (void)aInstance;
    (void)aBalance;
    CALL_HLE_DISPATCH(0x4D);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamGetBalance, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x4E);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamBytesRendered, TAny *aInstance, TUint64 &aBytesRendered) {
    (void)aInstance;
    (void)aBytesRendered;

    CALL_HLE_DISPATCH(0x4F);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamPosition, TAny *aInstance, TUint64 &aPosition) {
    (void)aInstance;
    (void)aPosition;

    CALL_HLE_DISPATCH(0x50);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamGetFormat, TAny *aInstance, TUint32 &aFourCC) {
    (void)aInstance;
    (void)aFourCC;

    CALL_HLE_DISPATCH(0x51);
}

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamCancelNotifyBufferSentToDriver, TAny *aInstance) {
    (void)aInstance;
    CALL_HLE_DISPATCH(0x52);
}