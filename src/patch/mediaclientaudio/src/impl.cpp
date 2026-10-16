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
#include <f32file.h>

static const TUint32 KBlackListAudioClipFormat[2] = { KUidMdaClipFormatRawAudioDefine, KUidMdaClipFormatAuDefine };
static const TUint32 KBlackListAudioClipFormatCount = sizeof(KBlackListAudioClipFormat) / sizeof(TUint32);

static const TInt KOpenerPriority = 100;
static const TInt KToneOpenerPriority = KOpenerPriority + 1;

CMMFMdaAudioOpenComplete::CMMFMdaAudioOpenComplete()
    : CIdle(KOpenerPriority)
    , iIsFixup(EFalse)
    , iTarget(NULL)
    , iIsTone(EFalse)
    , iTargetState(EMdaStateReady)
    , iPreviousState(EMdaStateIdle)
    , iStateApplied(EFalse)
    , iError(KErrNone) {
}

CMMFMdaAudioOpenComplete::~CMMFMdaAudioOpenComplete() {
    iIsFixup = EFalse;
    Deque();
}

static TInt OpenCompleteCallback(void *aUserdata) {
    CMMFMdaAudioOpenComplete *iOpenComplete = reinterpret_cast<CMMFMdaAudioOpenComplete *>(aUserdata);
    if (iOpenComplete != NULL) {
        iOpenComplete->CompleteOpen();
    }
    return 0;
}

void CMMFMdaAudioOpenComplete::CompleteOpen() {
    LogOut(KMcaCat, _L("Open utility/recorder/tone complete"));

    if (iIsTone) {
        CMMFMdaAudioToneUtility *stream = reinterpret_cast<CMMFMdaAudioToneUtility *>(iTarget);
        stream->CompletePrepare(KErrNone);
    } else {
        CMMFMdaAudioUtility *stream = reinterpret_cast<CMMFMdaAudioUtility *>(iTarget);

        if (iStateApplied) {
            stream->OnStateChanged(iTargetState, iPreviousState, iError);
        } else {
            stream->TransitionState(iTargetState, iError);
        }
    }
}

void CMMFMdaAudioOpenComplete::Open(CMMFMdaAudioUtility *aUtil) {
    Complete(aUtil, EMdaStateReady, KErrNone);
}

void CMMFMdaAudioOpenComplete::Complete(CMMFMdaAudioUtility *aUtil, const TMdaState aTargetState, const TInt aError) {
    iTarget = aUtil;
    iIsTone = EFalse;
    iTargetState = aTargetState;
    iStateApplied = EFalse;
    iError = aError;

    Start(TCallBack(OpenCompleteCallback, this));
}

void CMMFMdaAudioOpenComplete::Notify(CMMFMdaAudioUtility *aUtil, const TMdaState aPreviousState, const TMdaState aCurrentState, const TInt aError) {
    if (IsActive()) {
        // A pending open must still complete; an older notification is superseded.
        if (!IsNotifying()) {
            return;
        }

        FixupActiveStatus();
    }

    iTarget = aUtil;
    iIsTone = EFalse;
    iTargetState = aCurrentState;
    iPreviousState = aPreviousState;
    iStateApplied = ETrue;
    iError = aError;

    Start(TCallBack(OpenCompleteCallback, this));
}

void CMMFMdaAudioOpenComplete::FlushNotify() {
    if (IsNotifying()) {
        FixupActiveStatus();
        CompleteOpen();
    }
}

void CMMFMdaAudioOpenComplete::DiscardNotify() {
    if (IsNotifying()) {
        FixupActiveStatus();
    }
}

void CMMFMdaAudioOpenComplete::Open(CMMFMdaAudioToneUtility *aUtil) {
    iTarget = aUtil;
    iIsTone = ETrue;
    iStateApplied = EFalse;

    Start(TCallBack(OpenCompleteCallback, this));
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
    , iDuration(0)
    , iPosition(0)
    , iRecordTimeAvailable(0) {
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
    // This object outranks the notifier, so a completion can arrive before the start is reported.
    iOpener.FlushNotify();
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

void CMMFMdaAudioUtility::OpenClip(const TDesC &aFilename, const TInt aSize) {
    iOpener.FixupActiveStatus();
    iClipName.Copy(aFilename.Left(iClipName.MaxLength()));

    // A clip the host cannot record into may still be played back.
    if (EAudioPlayerOpenRecordClip(0, iDispatchInstance, iClipName.Ptr(), iClipName.Length()) != KErrNone) {
        LogOut(KMcaCat, _L("Clip %S can not be recorded into"), &iClipName);
    }

    if (aSize == 0) {
        iDuration = TTimeIntervalMicroSeconds(0);
        iPlayType = EMdaPlayTypeFile;
        iOpener.Complete(this, EMdaStateReady, KErrNone);

        return;
    }

    TRAPD(err, SupplyUrlL(aFilename));

    if (err != KErrNone) {
        iOpener.Complete(this, EMdaStateIdle, err);
    }
}

const TTimeIntervalMicroSeconds &CMMFMdaAudioUtility::RecordTimeAvailable() {
    iRecordTimeAvailable = TTimeIntervalMicroSeconds(0);

    TInt drive = 0;
    TParsePtrC parse(iClipName);

    RFs fs;
    if (!parse.DrivePresent() || (RFs::CharToDrive(parse.Drive()[0], drive) != KErrNone) || (fs.Connect() != KErrNone)) {
        return iRecordTimeAvailable;
    }

    const TInt bitRate = EAudioPlayerGetDestinationBitRate(0, iDispatchInstance);

    TVolumeInfo info;
    if ((bitRate > 0) && (fs.Volume(info, drive) == KErrNone)) {
        iRecordTimeAvailable = TTimeIntervalMicroSeconds(info.iFree * TInt64(8000000) / TInt64(bitRate));
    }

    fs.Close();
    return iRecordTimeAvailable;
}

void CMMFMdaAudioUtility::Record() {
    if (iState != EMdaStateReady) {
        iOpener.Notify(this, iState, iState, KErrNotReady);
        return;
    }

    const TInt err = EAudioPlayerRecord(0, iDispatchInstance, iClipName.Ptr(), iClipName.Length());

    if (err != KErrNone) {
        iOpener.Notify(this, EMdaStateReady, EMdaStateReady, err);
        return;
    }

    iState = EMdaStateRecord;
    iOpener.Notify(this, EMdaStateReady, EMdaStateRecord, KErrNone);
}

void CMMFMdaAudioUtility::ReloadRecordedClip() {
    TUint64 localDuration = 0;

    if (EAudioPlayerSupplyUrl(0, iDispatchInstance, iClipName.Ptr(), iClipName.Length(), &localDuration) != KErrNone) {
        LogOut(KMcaCat, _L("Unable to reopen the recorded clip for playback"));
        return;
    }

#ifndef EKA2
    iDuration = TTimeIntervalMicroSeconds(MakeSoftwareInt64FromHardwareUint64(localDuration));
#else
    iDuration = TTimeIntervalMicroSeconds(localDuration);
#endif
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

            // MMF fails Prime() on an unopened controller and reports it as an async play completion;
            // observers may call Play() again from that callback.
            iOpener.Notify(this, EMdaStatePlay, EMdaStateReady, KErrNotReady);
            return;
        }
    }

    TInt err = EAudioPlayerPlay(0, iDispatchInstance);

    if (err != KErrNone) {
        Cancel();
        TransitionState(EMdaStateReady, err);
        return;
    }

    // Callers commonly start waiting for the state change right after the call.
    const TMdaState previousState = iState;
    iState = EMdaStatePlay;
    iOpener.Notify(this, previousState, EMdaStatePlay, KErrNone);
}

void CMMFMdaAudioUtility::Stop() {
    // Stop reports nothing, so a start the observer has not heard of yet never happened for it.
    iOpener.DiscardNotify();

    if (iState == EMdaStateRecord) {
        EAudioPlayerStop(0, iDispatchInstance);
        iState = EMdaStateReady;

        Cancel();
        ReloadRecordedClip();

        return;
    }

    if ((iState == EMdaStatePlay) || (iState == EMdaStateRecord) || (iState == EMdaStatePause)) {
        EAudioPlayerStop(0, iDispatchInstance);
        iState = EMdaStateReady;
    }

    Cancel();
}

void CMMFMdaAudioUtility::Close() {
    iOpener.DiscardNotify();
    iState = EMdaStateIdle;
    EAudioPlayerStop(0, iDispatchInstance);
    Cancel();
}

TInt CMMFMdaAudioUtility::Pause() {
    iOpener.FlushNotify();

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

const TTimeIntervalMicroSeconds &CMMFMdaAudioUtility::CurrentPosition() {
    TUint64 result = 0;
    EAudioPlayerGetCurrentPlayPos(0, iDispatchInstance, result);

#ifdef EKA2
    iPosition = TTimeIntervalMicroSeconds(result);
#else
    iPosition = TTimeIntervalMicroSeconds(MakeSoftwareInt64FromHardwareUint64(result));
#endif

    // While recording, the clip grows with the captured audio.
    if (iState == EMdaStateRecord) {
        iDuration = iPosition;
    }

    return iPosition;
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

TInt CMMFMdaAudioRecorderUtility::SetDestBitRate(const TUint aBitRate) {
    return EAudioPlayerSetDestinationBitRate(0, iDispatchInstance, aBitRate);
}

TInt CMMFMdaAudioRecorderUtility::GetDestBitRate() {
    return EAudioPlayerGetDestinationBitRate(0, iDispatchInstance);
}

void CMMFMdaAudioRecorderUtility::GetSupportedDestValuesL(const TInt aKind, RArray<TUint> &aValues) {
    aValues.Reset();

    TUint32 values[16];
    const TInt count = EAudioPlayerGetSupportedDestinationValues(0, iDispatchInstance, aKind, values, 16);

    User::LeaveIfError(count);

    for (TInt i = 0; i < Min(count, 16); i++) {
        User::LeaveIfError(aValues.Append(values[i]));
    }
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

CMMFMdaAudioToneUtility::CMMFMdaAudioToneUtility(MMdaAudioToneObserver &aObserver, const TInt aPriority, const TMdaPriorityPreference aPref)
    : CActive(CActive::EPriorityStandard)
    , iObserver(aObserver)
    , iState(EMdaAudioToneUtilityNotReady)
    , iDispatchInstance(NULL) {
}

void CMMFMdaAudioToneUtility::ConstructL() {
    iDispatchInstance = ETonePlayerNewInstance(0);

    if (!iDispatchInstance) {
        User::Leave(KErrGeneral);
    }

    iOpener.FixupActiveStatus();

    // DevSound prepares a tone near-instantly; clients play it right after a pending clip open completes.
    iOpener.SetPriority(KToneOpenerPriority);

    CActiveScheduler::Add(&iOpener);
    CActiveScheduler::Add(this);
}

CMMFMdaAudioToneUtility::~CMMFMdaAudioToneUtility() {
    Cancel();
    ETonePlayerDestroy(0, iDispatchInstance);
}

CMMFMdaAudioToneUtility *CMMFMdaAudioToneUtility::NewL(MMdaAudioToneObserver &aObserver, const TInt aPriority, const TMdaPriorityPreference aPref) {
    CMMFMdaAudioToneUtility *self = new (ELeave) CMMFMdaAudioToneUtility(aObserver, aPriority, aPref);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);

    return self;
}

TInt CMMFMdaAudioToneUtility::SetVolume(const TInt aNewVolume) {
    return ETonePlayerSetVolume(0, iDispatchInstance, aNewVolume);
}

TInt CMMFMdaAudioToneUtility::GetVolume() {
    return ETonePlayerGetVolume(0, iDispatchInstance);
}

TInt CMMFMdaAudioToneUtility::MaxVolume() {
    return ETonePlayerMaxVolume(0, iDispatchInstance);
}

TInt CMMFMdaAudioToneUtility::GetBalance() {
    return ETonePlayerGetBalance(0, iDispatchInstance);
}

TInt CMMFMdaAudioToneUtility::SetBalance(const TInt aBalance) {
    return ETonePlayerSetBalance(0, iDispatchInstance, aBalance);
}

#define CALL_PREARE(function, ARGS...)                                                  \
    iOpener.FixupActiveStatus();                                                        \
    TInt err = function(0, iDispatchInstance, ##ARGS);                                  \
    if (err != KErrNone) {                                                          \
        CompletePrepare(err);                                                       \
    } else {                                                                        \
        iOpener.Open(this);                                                         \
}

void CMMFMdaAudioToneUtility::PrepareToPlayFileSequence(const TDesC &aFileName) {
    CALL_PREARE(ETonePlayerSetFileToneSequence, aFileName)
}

void CMMFMdaAudioToneUtility::PrepareToPlayBufferSequence(const TDesC8 &aBuf) {
    CALL_PREARE(ETonePlayerSetBufferToneSequence, aBuf)
}

void CMMFMdaAudioToneUtility::PrepareToPlayFixedSequence(const TInt aSequenceIndex) {
    CALL_PREARE(ETonePlayerSetFixedToneSequence, aSequenceIndex)
}

void CMMFMdaAudioToneUtility::PrepareToPlayTone(const TInt aFreq, const TTimeIntervalMicroSeconds &aDuration) {
#ifdef EKA2
    CALL_PREARE(ETonePlayerSetSingleTone, aFreq, aDuration.Int64() & 0xFFFFFFFF, (aDuration.Int64() >> 32) & 0xFFFFFFFF)
#else
    CALL_PREARE(ETonePlayerSetSingleTone, aFreq, aDuration.Int64().GetTInt(), 0)
#endif
}

void CMMFMdaAudioToneUtility::PrepareToPlayDualTone(const TInt aFreqOne, const TInt aFreqTwo, const TTimeIntervalMicroSeconds &aDuration) {
#ifdef EKA2
    CALL_PREARE(ETonePlayerSetDualTone, aFreqOne, aFreqTwo, aDuration.Int64() & 0xFFFFFFFF, (aDuration.Int64() >> 32) & 0xFFFFFFFF)
#else
    CALL_PREARE(ETonePlayerSetDualTone, aFreqOne, aFreqTwo, aDuration.Int64().GetTInt(), 0)
#endif
}

void CMMFMdaAudioToneUtility::PrepareToPlayDTMFString(const TDesC &aDTMF) {
    CALL_PREARE(ETonePlayerSetDTMFString, aDTMF)
}

void CMMFMdaAudioToneUtility::SetDTMFLengths(const TInt aToneOnMicros, const TInt aToneOffMicros, const TInt aPauseMicros) {
    ETonePlayerSetDTMFLengths(0, iDispatchInstance, aToneOnMicros, aToneOffMicros, aPauseMicros);
}

void CMMFMdaAudioToneUtility::CancelPrepare() {
    // Not gonna do anything :)
}

void CMMFMdaAudioToneUtility::Play() {
    if (iState == EMdaAudioToneUtilityPlaying) {
        LogOut(KMcaCat, _L("Trying to call play while tone player is playing!"));
        return;
    }

    // Will override existing
    iStatus = KRequestPending;

    ETonePlayerNotifyAnyDone(0, iDispatchInstance, iStatus);
    SetActive();

    TInt err = ETonePlayerPlay(0, iDispatchInstance);
    if (err != KErrNone) {
        LogOut(KMcaCat, _L("Playing tone player encountered error: %d"), err);
        iObserver.MatoPlayComplete(err);
    } else {
        iState = EMdaAudioToneUtilityPlaying;
    }
}

void CMMFMdaAudioToneUtility::CancelPlay() {
    Cancel();
}

TInt CMMFMdaAudioToneUtility::FixedSequenceCount() const {
    return ETonePlayerGetFixedSequenceCount(0, iDispatchInstance);
}

void CMMFMdaAudioToneUtility::SetRepeats(const TInt aHowManyTimes, const TTimeIntervalMicroSeconds &aSilenceInterval) {
#ifdef EKA2
    ETonePlayerSetRepeats(0, iDispatchInstance, aHowManyTimes, aSilenceInterval.Int64() & 0xFFFFFFFF, (aSilenceInterval.Int64() >> 32) & 0xFFFFFFFF);
#else
    ETonePlayerSetRepeats(0, iDispatchInstance, aHowManyTimes, aSilenceInterval.Int64().GetTInt(), 0);
#endif
}

const TMdaAudioToneUtilityState CMMFMdaAudioToneUtility::State() {
    return iState;
}

void CMMFMdaAudioToneUtility::RunL() {
    iState = EMdaAudioToneUtilityPrepared;
    iObserver.MatoPlayComplete(iStatus.Int());
}

// CancelPlay reports nothing to the observer (CMMFMdaAudioToneUtility::ToneFinished skips KErrCancel).
void CMMFMdaAudioToneUtility::DoCancel() {
    ETonePlayerStop(0, iDispatchInstance);
    iState = EMdaAudioToneUtilityPrepared;
}

void CMMFMdaAudioToneUtility::CompletePrepare(TInt aError) {
    if (aError == KErrNone) {
        iState = EMdaAudioToneUtilityPrepared;
    }

    iObserver.MatoPrepareComplete(aError);
}
