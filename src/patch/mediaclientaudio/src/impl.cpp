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

#include <Log.h>
#include <SoftInt.h>

#include "dispatch.h"
#include "impl.h"

#ifdef EKA2
#include <e32cmn.h>
#endif

#include <e32def.h>
#include <e32std.h>

static const TUint32 KBlackListAudioClipFormat[2] = { KUidMdaClipFormatRawAudioDefine, KUidMdaClipFormatAuDefine };
static const TUint32 KBlackListAudioClipFormatCount = sizeof(KBlackListAudioClipFormat) / sizeof(TUint32);

CMMFMdaAudioOpenComplete::CMMFMdaAudioOpenComplete()
    : CIdle(100)
    , iIsFixup(EFalse) {
}

CMMFMdaAudioOpenComplete::~CMMFMdaAudioOpenComplete() {
    iIsFixup = EFalse;
    Deque();
}

static TInt OpenCompleteCallback(void *aUserdata) {
    LogOut(KMcaCat, _L("Open utility/recorder complete"));

    CMMFMdaAudioUtility *stream = reinterpret_cast<CMMFMdaAudioUtility *>(aUserdata);
    stream->TransitionState(EMdaStateReady, KErrNone);

    return 0;
}

void CMMFMdaAudioOpenComplete::Open(CMMFMdaAudioUtility *aUtil) {
    Start(TCallBack(OpenCompleteCallback, aUtil));
}

void CMMFMdaAudioOpenComplete::FixupActiveStatus() {
    iIsFixup = ETrue;

    if (IsActive()) {
        Cancel();
    }

    iIsFixup = EFalse;
}

void CMMFMdaAudioOpenComplete::DoCancel() {
    if (iIsFixup) {
        TRequestStatus *statusPointer = &iStatus;
        User::RequestComplete(statusPointer, KErrCancel);
    }
}

CMMFMdaAudioUtility::CMMFMdaAudioUtility(const TInt aPriority, const TMdaPriorityPreference aPref)
    : CActive(10000)
    , iPriority(aPriority)
    , iPref(aPref)
    , iState(EMdaStateIdle)
    , iDuration(0) {
}

void CMMFMdaAudioUtility::ConstructL(const TUint32 aInitFlags) {
    iDispatchInstance = EAudioPlayerNewInstance(0, aInitFlags);

    if (!iDispatchInstance) {
        User::Leave(KErrGeneral);
    }

    iOpener.FixupActiveStatus();

    CActiveScheduler::Add(&iOpener);
    CActiveScheduler::Add(this);
}

void CMMFMdaAudioUtility::TransitionState(const TMdaState aNewState, const TInt aError) {
    const TMdaState previousState = iState;

    if (aError >= KErrNone) {
        iState = aNewState;
    }

    OnStateChanged(aNewState, previousState, aError);
}

CMMFMdaAudioUtility::~CMMFMdaAudioUtility() {
    Deque();
    EAudioPlayerDestroy(0, iDispatchInstance);
}

void CMMFMdaAudioUtility::RunL() {
    TransitionState(EMdaStateReady);
}

void CMMFMdaAudioUtility::DoCancel() {
    EAudioPlayerCancelNotifyAnyDone(0, iDispatchInstance);
}

void CMMFMdaAudioUtility::StartListeningForCompletion() {
    iStatus = KRequestPending;

    EAudioPlayerNotifyAnyDone(0, iDispatchInstance, iStatus);
    SetActive();
}

void CMMFMdaAudioUtility::SupplyUrlL(const TDesC &aFilename) {
    if (iState == EMdaStateReady) {
        LogOut(KMcaCat, _L("Audio clip player must be closed using Close() function before open new audio clip!"));
        TransitionState(iState, KErrInUse);

        return;
    }

    if ((iState == EMdaStatePlay) || (iState == EMdaStatePause)) {
        LogOut(KMcaCat, _L("Audio supplied while utility is being played."));
        TransitionState(iState, KErrInUse);

        return;
    }

    iOpener.FixupActiveStatus();

    TUint64 localDuration = 0;
    User::LeaveIfError(EAudioPlayerSupplyUrl(0, iDispatchInstance, aFilename.Ptr(), aFilename.Length(), &localDuration));

#ifndef EKA2
    iDuration = TTimeIntervalMicroSeconds(MakeSoftwareInt64FromHardwareUint64(localDuration));
#else
    iDuration = TTimeIntervalMicroSeconds(localDuration);
#endif

    iPlayType = EMdaPlayTypeFile;

    iOpener.Open(this);
}

void CMMFMdaAudioUtility::SupplyDataL(const TDesC8 &aData) {
    if (iState == EMdaStateReady) {
        LogOut(KMcaCat, _L("Audio clip player must be closed using Close() function before open new audio clip!"));
        TransitionState(iState, KErrInUse);

        return;
    }

    if ((iState == EMdaStatePlay) || (iState == EMdaStatePause)) {
        LogOut(KMcaCat, _L("Audio supplied while utility is being played."));

        TransitionState(iState, KErrInUse);
        return;
    }

    // Cancel if it's being activated
    iOpener.FixupActiveStatus();

    TUint64 localDuration = 0;
    User::LeaveIfError(EAudioPlayerSupplyData(0, iDispatchInstance, aData, &localDuration));

#ifndef EKA2
    iDuration = TTimeIntervalMicroSeconds(MakeSoftwareInt64FromHardwareUint64(localDuration));
#else
    iDuration = TTimeIntervalMicroSeconds(localDuration);
#endif

    iPlayType = EMdaPlayTypeBuffer;

    iOpener.Open(this);
}

void CMMFMdaAudioUtility::Play() {
    if (iState != EMdaStatePause) {
        if (iState == EMdaStatePlay) {
            Cancel();
        }

        if ((iState == EMdaStateReady) || (iState == EMdaStatePlay)) {
            StartListeningForCompletion();
        } else {
            LogOut(KMcaCat, _L("Can't play audio due to no data not being yet queued."));

            TransitionState(EMdaStateReady, KErrNotReady);
            return;
        }
    }

    TransitionState(EMdaStatePlay, KErrNone);
    TInt err = EAudioPlayerPlay(0, iDispatchInstance);

    if (err != KErrNone) {
        Cancel();
        TransitionState(EMdaStateReady, err);
    }
}

void CMMFMdaAudioUtility::Stop() {
    TransitionState(EMdaStateReady, EAudioPlayerStop(0, iDispatchInstance) ? 2 : 1);

    // Do cancel
    Cancel();
}

void CMMFMdaAudioUtility::Close() {
    // Closing a clip will of course first will put a stop to playing
    // The emulator side will auto handle closing when open a new URL. Here we are just complying
    // with the rules...
    TransitionState(EMdaStateIdle, KErrNone);
    EAudioPlayerStop(0, iDispatchInstance);

    // Do cancel
    Cancel();
}

TInt CMMFMdaAudioUtility::Pause() {
    const TInt result = EAudioPlayerPause(0, iDispatchInstance);

    if (result != KErrNone) {
        return result;
    }

    TransitionState(EMdaStatePause);
    return KErrNone;
}

TInt CMMFMdaAudioUtility::SetVolume(const TInt aNewVolume) {
    return EAudioPlayerSetVolume(0, iDispatchInstance, aNewVolume);
}

TInt CMMFMdaAudioUtility::MaxVolume() {
    return EAudioPlayerMaxVolume(0, iDispatchInstance);
}

TInt CMMFMdaAudioUtility::GetVolume() {
    return EAudioPlayerGetVolume(0, iDispatchInstance);
}

TTimeIntervalMicroSeconds CMMFMdaAudioUtility::CurrentPosition() {
#ifdef EKA2
    TUint64 result = 0;
    EAudioPlayerGetCurrentPlayPos(0, iDispatchInstance, result);

    return result;
#else
    TUint64 result = 0;
    EAudioPlayerGetCurrentPlayPos(0, iDispatchInstance, result);
    return TTimeIntervalMicroSeconds(MakeSoftwareInt64FromHardwareUint64(result));
#endif
}

void CMMFMdaAudioUtility::SetCurrentPosition(const TTimeIntervalMicroSeconds &aPos) {
    EAudioPlayerSetCurrentPlayPos(0, iDispatchInstance, aPos.Int64());
}

TInt CMMFMdaAudioUtility::BitRate(TUint &aBitRate) {
    TInt bitrate = EAudioPlayerGetCurrentBitRate(0, iDispatchInstance);

    if (bitrate < KErrNone) {
        return bitrate;
    }

    aBitRate = bitrate;
    return KErrNone;
}

TInt CMMFMdaAudioUtility::GetBalance() {
    return EAudioPlayerGetBalance(0, iDispatchInstance);
}

TInt CMMFMdaAudioUtility::SetBalance(const TInt aBalance) {
    return EAudioPlayerSetBalance(0, iDispatchInstance, aBalance);
}

void CMMFMdaAudioUtility::SetRepeats(const TInt aHowManyTimes, const TTimeIntervalMicroSeconds &aSilenceInterval) {
#ifdef EKA2
    EAudioPlayerSetRepeats(0, iDispatchInstance, aHowManyTimes, aSilenceInterval.Int64() & 0xFFFFFFFF, (aSilenceInterval.Int64() >> 32) & 0xFFFFFFFF);
#else
    EAudioPlayerSetRepeats(0, iDispatchInstance, aHowManyTimes, aSilenceInterval.Int64().GetTInt(), 0);
#endif
}

///================== AUDIO PLAYER UTILITY ======================
CMMFMdaAudioPlayerUtility::CMMFMdaAudioPlayerUtility(MMdaAudioPlayerCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref)
    : CMMFMdaAudioUtility(aPriority, aPref)
    , iCallback(aCallback) {
}

CMMFMdaAudioPlayerUtility *CMMFMdaAudioPlayerUtility::NewL(MMdaAudioPlayerCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref) {
    CMMFMdaAudioPlayerUtility *newUtil = new (ELeave) CMMFMdaAudioPlayerUtility(aCallback, aPriority, aPref);
    CleanupStack::PushL(newUtil);
    newUtil->ConstructL(0);
    CleanupStack::Pop(newUtil);

    return newUtil;
}

void CMMFMdaAudioPlayerUtility::OnStateChanged(const TMdaState aCurrentState, const TMdaState aPrevState, const TInt aErr) {
    switch (aCurrentState) {
    case EMdaStateReady:
        if (aPrevState != EMdaStateIdle) {
            if (aErr < 1) {
                iCallback.MapcPlayComplete(aErr);
            }
        } else {
            iCallback.MapcInitComplete(aErr, iDuration);
        }

        break;

    default:
        break;
    }
}

///================== AUDIO RECORDER UTILITY =======================
CMMFMdaAudioRecorderUtility::CMMFMdaAudioRecorderUtility(MMdaObjectStateChangeObserver &aObserver, const TInt aPriority, const TMdaPriorityPreference aPref)
    : CMMFMdaAudioUtility(aPriority, aPref)
    , iObserver(aObserver)
    , iContainerFormat(0) {
}

CMMFMdaAudioRecorderUtility *CMMFMdaAudioRecorderUtility::NewL(MMdaObjectStateChangeObserver &aObserver, const TInt aPriority, const TMdaPriorityPreference aPref) {
    CMMFMdaAudioRecorderUtility *newUtil = new (ELeave) CMMFMdaAudioRecorderUtility(aObserver, aPriority, aPref);
    CleanupStack::PushL(newUtil);
    newUtil->ConstructL(0);
    CleanupStack::Pop(newUtil);

    return newUtil;
}

bool TranslateInternalStateToReleasedState(const TMdaState aState, CMdaAudioClipUtility::TState &aReleasedState) {
    switch (aState) {
    case EMdaStateIdle:
        aReleasedState = CMdaAudioClipUtility::ENotReady;
        break;

    case EMdaStateReady:
        aReleasedState = CMdaAudioClipUtility::EOpen;
        break;

    case EMdaStatePlay:
        aReleasedState = CMdaAudioClipUtility::EPlaying;
        break;

    case EMdaStateRecord:
        aReleasedState = CMdaAudioClipUtility::ERecording;
        break;

    default:
        return false;
    }

    return true;
}

void CMMFMdaAudioRecorderUtility::OnStateChanged(const TMdaState aCurrentState, const TMdaState aPreviousState, const TInt aError) {
    CMdaAudioClipUtility::TState currentTrans, prevTrans;
    if (!TranslateInternalStateToReleasedState(aCurrentState, currentTrans) || !TranslateInternalStateToReleasedState(aPreviousState, prevTrans)) {
        return;
    }

    TInt realError = aError;
    if (aError == 1)  {
        realError = 0;
    } else if ((aError == 2) && (currentTrans == CMdaAudioClipUtility::EOpen)) {
        // Cite from Symbian OS C++ for Mobile Phones: Programming with Extended Functionality, page 269 and page 267
        // If record/play complete, call again with KErrNone signaling manual Stop()
        // And in page 269 it says signal again with the ERecording as the current state. The situation though does seems sure to be changed in S60v3
        currentTrans = prevTrans;
        realError = 0;
    }

    iObserver.MoscoStateChangeEvent(this, prevTrans, currentTrans, realError);
}

TInt CMMFMdaAudioRecorderUtility::SetDestCodec(TFourCC aDestCodec) {
    return EAudioPlayerSetDestinationEncoding(0, iDispatchInstance, aDestCodec.FourCC());
}

TInt CMMFMdaAudioRecorderUtility::GetDestCodec(TFourCC &aDestCodec) {
    TUint32 codec = 0;
    const TInt res = EAudioPlayerGetDestinationEncoding(0, iDispatchInstance, codec);

    aDestCodec.Set(codec);
    return res;
}

TInt CMMFMdaAudioRecorderUtility::SetDestChannelCount(const TInt aChannelCount) {
    return EAudioPlayerSetDestinationChannelCount(0, iDispatchInstance, aChannelCount);
}

TInt CMMFMdaAudioRecorderUtility::SetDestSampleRate(const TInt aSampleRate) {
    return EAudioPlayerSetDestinationSampleRate(0, iDispatchInstance, aSampleRate);
}

TInt CMMFMdaAudioRecorderUtility::GetDestChannelCount() {
    return EAudioPlayerGetDestinationChannelCount(0, iDispatchInstance);
}

TInt CMMFMdaAudioRecorderUtility::GetDestSampleRate() {
    return EAudioPlayerGetDestinationSampleRate(0, iDispatchInstance);
}

TInt CMMFMdaAudioRecorderUtility::SetDestContainerFormat(const TUint32 aUid) {
    // Check if the audio clip format is in our blacklist first
    for (TUint32 i = 0; i < KBlackListAudioClipFormatCount; i++) {
        if (aUid == KBlackListAudioClipFormat[i]) {
            LogOut(KMcaCat, _L("Unsupport audio format with UID 0x%08x"), aUid);
            return KErrNotSupported;
        }
    }

    // Translate audio format UID to FourCC
    TFourCC audioFormatCC;

    switch (aUid) {
    case KUidMdaClipFormatRawAmrDefine:
        audioFormatCC = TFourCC(' ', 'A', 'M', 'R');
        break;

    case KUidMdaClipFormatWavDefine:
        audioFormatCC = TFourCC(' ', 'W', 'A', 'V');
        break;

    default:
        LogOut(KMcaCat, _L("Audio format to translate to FourCC undefined 0x%08x"), aUid);
        return KErrNotSupported;
    }

    iContainerFormat = aUid;
    return KErrNone;
}

TInt CMMFMdaAudioRecorderUtility::GetDestContainerFormat(TUint32 &aUid) {
    aUid = iContainerFormat;
    return KErrNone;
}
