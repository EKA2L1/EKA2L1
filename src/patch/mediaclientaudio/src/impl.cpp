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

static const TUint32 KBlackListAudioClipFormat[2] = { KUidMdaClipFormatRawAudioDefine, KUidMdaClipFormatAuDefine };
static const TUint32 KBlackListAudioClipFormatCount = sizeof(KBlackListAudioClipFormat) / sizeof(TUint32);

CMMFMdaAudioUtility::CMMFMdaAudioUtility(const TInt aPriority, const TMdaPriorityPreference aPref)
    : CActive(CActive::EPriorityIdle)
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
    EAudioPlayerNotifyAnyDone(0, iDispatchInstance, iStatus);
    SetActive();
}

void CMMFMdaAudioUtility::SupplyUrl(const TDesC &aFilename) {
    if ((iState == EMdaStatePlay) || (iState == EMdaStatePause)) {
        User::Leave(KErrInUse);
    }

    TInt duration = EAudioPlayerSupplyUrl(0, iDispatchInstance, aFilename.Ptr(), aFilename.Length());
    TTimeIntervalMicroSeconds durationObj = TTimeIntervalMicroSeconds(duration);

    iDuration = durationObj;
    iPlayType = EMdaPlayTypeFile;

    TransitionState(EMdaStateReady, duration < KErrNone ? duration : KErrNone);
}

void CMMFMdaAudioUtility::SupplyData(TDesC8 &aData) {
    if ((iState == EMdaStatePlay) || (iState == EMdaStatePause)) {
        User::Leave(KErrInUse);
    }

    TInt duration = EAudioPlayerSupplyData(0, iDispatchInstance, aData);
    TTimeIntervalMicroSeconds durationObj = TTimeIntervalMicroSeconds(duration);

    iDuration = durationObj;
    iPlayType = EMdaPlayTypeBuffer;
    
    TransitionState(EMdaStateReady, duration < KErrNone ? duration : KErrNone);
}

void CMMFMdaAudioUtility::Play() {
    if (iState == EMdaStateReady) {
        StartListeningForCompletion();
    }

    TInt err = EAudioPlayerPlay(0, iDispatchInstance);
    TransitionState(EMdaStatePlay, err);
}

void CMMFMdaAudioUtility::Stop() {
	TransitionState(EMdaStateReady, 1);
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
    return EAudioPlayerGetCurrentPlayPos(0, iDispatchInstance);
#else
    return TTimeIntervalMicroSeconds(TInt64(static_cast<TInt>(EAudioPlayerGetCurrentPlayPos(0, iDispatchInstance))));
#endif
}

void CMMFMdaAudioUtility::SetCurrentPosition(const TTimeIntervalMicroSeconds &aPos) {
#ifdef EKA2
    EAudioPlayerSetCurrentPlayPos(0, iDispatchInstance, aPos.Int64());
#else
    EAudioPlayerSetCurrentPlayPos(0, iDispatchInstance, aPos.Int64().GetTInt());
#endif
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
    EAudioPlayerSetRepeats(0, iDispatchInstance, aHowManyTimes, aSilenceInterval.Int64());
#else
    EAudioPlayerSetRepeats(0, iDispatchInstance, aHowManyTimes, aSilenceInterval.Int64().GetTInt());
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
    newUtil->ConstructL(EAudioPlayerFlag_StartPreparePlayWhenQueue);
    CleanupStack::Pop(newUtil);

    return newUtil;
}

void CMMFMdaAudioPlayerUtility::OnStateChanged(const TMdaState aCurrentState, const TMdaState aPrevState, const TInt aErr) {
	switch (aCurrentState) {
		case EMdaStateReady:
			if (aErr != 1)
				iCallback.MapcInitComplete(aErr, iDuration);

			break;
			
		case EMdaStateIdle:
			iCallback.MapcPlayComplete(aErr);
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
	
	// TODO: no nullptr? Haha hehe
	iObserver.MoscoStateChangeEvent(NULL, currentTrans, prevTrans, aError);
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
			LogOut(MCA_CAT, _L("Unsupport audio format with UID 0x%08x"), aUid);
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
		LogOut(MCA_CAT, _L("Audio format to translate to FourCC undefined 0x%08x"), aUid);
		return KErrNotSupported;
	}

    iContainerFormat = aUid;
    return KErrNone;
}

TInt CMMFMdaAudioRecorderUtility::GetDestContainerFormat(TUint32 &aUid) {
    aUid = iContainerFormat;
    return KErrNone;
}
