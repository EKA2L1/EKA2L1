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

#include "dispatch.h"
#include "impl.h"
#include "log.h"

#ifdef EKA2
#include <e32cmn.h>
#endif

#include <e32def.h>
#include <e32std.h>

CMMFMdaAudioPlayerUtility::CMMFMdaAudioPlayerUtility(MMdaAudioPlayerCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref)
    : CActive(CActive::EPriorityIdle)
    , iCallback(aCallback)
    , iPriority(aPriority)
    , iPref(aPref)
    , iState(EMdaStateIdle)
    , iDuration(0) {
}

CMMFMdaAudioPlayerUtility *CMMFMdaAudioPlayerUtility::NewL(MMdaAudioPlayerCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref) {
    CMMFMdaAudioPlayerUtility *newUtil = new (ELeave) CMMFMdaAudioPlayerUtility(aCallback, aPriority, aPref);
    CleanupStack::PushL(newUtil);
    newUtil->ConstructL();
    CleanupStack::Pop(newUtil);

    return newUtil;
}

void CMMFMdaAudioPlayerUtility::ConstructL() {
    iDispatchInstance = EAudioPlayerNewInstance(0, NULL);

    if (!iDispatchInstance) {
        User::Leave(KErrGeneral);
    }

    CActiveScheduler::Add(this);
}

CMMFMdaAudioPlayerUtility::~CMMFMdaAudioPlayerUtility() {
    Deque();
    EAudioPlayerDestroy(0, iDispatchInstance);
}

void CMMFMdaAudioPlayerUtility::RunL() {
    iCallback.MapcPlayComplete(iStatus.Int());
    iState = EMdaStateIdle;
}

void CMMFMdaAudioPlayerUtility::DoCancel() {
    EAudioPlayerCancelNotifyAnyDone(0, iDispatchInstance);
}

void CMMFMdaAudioPlayerUtility::StartListeningForCompletion() {
    EAudioPlayerNotifyAnyDone(0, iDispatchInstance, iStatus);
    SetActive();
}

void CMMFMdaAudioPlayerUtility::SupplyUrl(const TDesC &aFilename) {
    if (iState == EMdaStateReady) {
        User::Leave(KErrInUse);
    }

    TInt duration = EAudioPlayerSupplyUrl(0, iDispatchInstance, aFilename.Ptr(), aFilename.Length());
    TTimeIntervalMicroSeconds durationObj = TTimeIntervalMicroSeconds(duration);

    if (duration < KErrNone) {
        iCallback.MapcInitComplete(duration, TTimeIntervalMicroSeconds(0));
        return;
    } else {
        iCallback.MapcInitComplete(KErrNone, durationObj);
    }

    iDuration = durationObj;
}

void CMMFMdaAudioPlayerUtility::Play() {
    if (iState == EMdaStateIdle) {
        StartListeningForCompletion();
    }

    iState = EMdaStatePlay;
    TInt err = EAudioPlayerPlay(0, iDispatchInstance);

    if (err != KErrNone) {
        // An error occured when trying to play. Returns immidiately
        iCallback.MapcPlayComplete(err);
    }
}

void CMMFMdaAudioPlayerUtility::Stop() {
    iState = EMdaStateIdle;
    EAudioPlayerStop(0, iDispatchInstance);

    // Do cancel
    Cancel();
}

TInt CMMFMdaAudioPlayerUtility::Pause() {
    const TInt result = EAudioPlayerPause(0, iDispatchInstance);

    if (result != KErrNone) {
        return result;
    }

    iState = EMdaStatePause;
    return KErrNone;
}

TInt CMMFMdaAudioPlayerUtility::SetVolume(const TInt aNewVolume) {
    return EAudioPlayerSetVolume(0, iDispatchInstance, aNewVolume);
}

TInt CMMFMdaAudioPlayerUtility::MaxVolume() {
    return EAudioPlayerMaxVolume(0, iDispatchInstance);
}

TInt CMMFMdaAudioPlayerUtility::GetVolume() {
    return EAudioPlayerGetVolume(0, iDispatchInstance);
}

TTimeIntervalMicroSeconds CMMFMdaAudioPlayerUtility::CurrentPosition() {
#ifdef EKA2
    return EAudioPlayerGetCurrentPlayPos(0, iDispatchInstance);
#else
    return TTimeIntervalMicroSeconds(TInt64(static_cast<TInt>(EAudioPlayerGetCurrentPlayPos(0, iDispatchInstance))));
#endif
}

void CMMFMdaAudioPlayerUtility::SetCurrentPosition(const TTimeIntervalMicroSeconds &aPos) {
#ifdef EKA2
    EAudioPlayerSetCurrentPlayPos(0, iDispatchInstance, aPos.Int64());
#else
    EAudioPlayerSetCurrentPlayPos(0, iDispatchInstance, aPos.Int64().GetTInt());
#endif
}

TInt CMMFMdaAudioPlayerUtility::BitRate(TUint &aBitRate) {
    TInt bitrate = EAudioPlayerGetCurrentBitRate(0, iDispatchInstance);

    if (bitrate < KErrNone) {
        return bitrate;
    }

    aBitRate = bitrate;
    return KErrNone;
}

TInt CMMFMdaAudioPlayerUtility::GetBalance() {
    return EAudioPlayerGetBalance(0, iDispatchInstance);
}

TInt CMMFMdaAudioPlayerUtility::SetBalance(const TInt aBalance) {
    return EAudioPlayerSetBalance(0, iDispatchInstance, aBalance);
}

void CMMFMdaAudioPlayerUtility::SetRepeats(const TInt aHowManyTimes, const TTimeIntervalMicroSeconds &aSilenceInterval) {
#ifdef EKA2
    EAudioPlayerSetRepeats(0, iDispatchInstance, aHowManyTimes, aSilenceInterval.Int64());
#else
    EAudioPlayerSetRepeats(0, iDispatchInstance, aHowManyTimes, aSilenceInterval.Int64().GetTInt());
#endif
}