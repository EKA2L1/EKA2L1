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

#ifndef MEDIA_CLIENT_AUDIO_IMPL_H_
#define MEDIA_CLIENT_AUDIO_IMPL_H_

#include <mdaaudiosampleeditor.h>
#include <mdaaudiosampleplayer.h>
#include <mdaaudiotoneplayer.h>

#include <e32base.h>
#include <e32std.h>

enum TMdaState {
    EMdaStateIdle = 0,
    EMdaStatePlay = 1,
    EMdaStateRecord = 2,
    EMdaStatePause = 3,
    EMdaStateReady = 4
};

enum TMdaPlayType {
    EMdaPlayTypeFile = 0,
    EMdaPlayTypeBuffer = 1
};

bool TranslateInternalStateToReleasedState(const TMdaState aState, CMdaAudioClipUtility::TState &aReleasedState);

struct CMMFMdaAudioUtility;
struct CMMFMdaAudioToneUtility;

struct CMMFMdaAudioOpenComplete : public CIdle {
protected:
    TBool iIsFixup;
    TAny *iTarget;
    TBool iIsTone;
    TMdaState iTargetState;
    TMdaState iPreviousState;
    TBool iStateApplied;
    TInt iError;

    TBool IsNotifying() const {
        return IsActive() && !iIsTone && iStateApplied;
    }

public:
    CMMFMdaAudioOpenComplete();
    ~CMMFMdaAudioOpenComplete();

    void Open(CMMFMdaAudioUtility *aUtil);
    void Complete(CMMFMdaAudioUtility *aUtil, const TMdaState aTargetState, const TInt aError);
    void Open(CMMFMdaAudioToneUtility *aToneUtility);
    void FixupActiveStatus();

    // Queues the observer callback for a transition already made, as the MMF client reports state changes.
    void Notify(CMMFMdaAudioUtility *aUtil, const TMdaState aPreviousState, const TMdaState aCurrentState, const TInt aError);
    void FlushNotify();
    void DiscardNotify();

    virtual void DoCancel();
    void CompleteOpen();
};

/**
 * @brief Utility class implementation. 
 *
 * This class both serves playing, recording and cropping audio.
 * Only either playing/recording is supported at once. Cropping is synchronous operation.
 */
struct CMMFMdaAudioUtility : public CActive {
protected:
    TInt iPriority;
    TMdaPriorityPreference iPref;
    TMdaPlayType iPlayType;

private:
    TMdaState iState;
    CMMFMdaAudioOpenComplete iOpener;

protected:
    TAny *iDispatchInstance;
    TTimeIntervalMicroSeconds iDuration;
    TTimeIntervalMicroSeconds iPosition;
    TTimeIntervalMicroSeconds iRecordTimeAvailable;
    TFileName iClipName;

    void ReloadRecordedClip();

public:
    CMMFMdaAudioUtility(const TInt aPriority, const TMdaPriorityPreference aPref);
    ~CMMFMdaAudioUtility();

    void TransitionState(const TMdaState aNewState, const TInt aError = KErrNone);
    void ConstructL(const TUint32 aInitFlags);

    virtual void RunL();
    virtual void DoCancel();

    void StartListeningForCompletion();

    void SupplyUrlL(const TDesC &aUrl);
    void SupplyDataL(const TDesC8 &aData);

    // An empty clip opens as a recording destination instead of being handed to the player.
    void OpenClip(const TDesC &aFilename, const TInt aSize);
    void Record();

    void Play();
    void Stop();
    void Close();

    TInt Pause();

    // Clip utility getters hand out references, so the values live in the utility.
    const TTimeIntervalMicroSeconds &CurrentPosition();
    void SetCurrentPosition(const TTimeIntervalMicroSeconds &aPos);

    TInt SetVolume(const TInt aNewVolume);
    TInt GetVolume();
    TInt MaxVolume();

    TInt BitRate(TUint &aBitRate);

    TInt GetBalance();
    TInt SetBalance(const TInt aBalance);

    const TTimeIntervalMicroSeconds &Duration() {
        return iDuration;
    }

    const TTimeIntervalMicroSeconds &RecordTimeAvailable();

    const TMdaState State() const {
        return iState;
    }

    void SetRepeats(const TInt aHowManyTimes, const TTimeIntervalMicroSeconds &aSilenceInterval);

    // Base hook for devired
    virtual void OnStateChanged(const TMdaState aCurrentState, const TMdaState aPreviousState, const TInt aError) = 0;
};

struct CMMFMdaAudioPlayerUtility : public CMMFMdaAudioUtility {
private:
    MMdaAudioPlayerCallback &iCallback;

public:
    CMMFMdaAudioPlayerUtility(MMdaAudioPlayerCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref);
    static CMMFMdaAudioPlayerUtility *NewL(MMdaAudioPlayerCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref);

    virtual void OnStateChanged(const TMdaState aCurrentState, const TMdaState aPreviousState, const TInt aError);
};

struct CMMFMdaAudioRecorderUtility : public CMMFMdaAudioUtility {
private:
    MMdaObjectStateChangeObserver &iObserver;
    TUint32 iContainerFormat;

public:
    CMMFMdaAudioRecorderUtility(MMdaObjectStateChangeObserver &aObserver, const TInt aPriority, const TMdaPriorityPreference aPref);
    static CMMFMdaAudioRecorderUtility *NewL(MMdaObjectStateChangeObserver &aObserver, const TInt aPriority, const TMdaPriorityPreference aPref);

    virtual void OnStateChanged(const TMdaState aCurrentState, const TMdaState aPreviousState, const TInt aError);

    TInt SetDestCodec(TFourCC aDestCodec);
    TInt GetDestCodec(TFourCC &aDestCodec);

    TInt SetDestChannelCount(const TInt aChannelCount);
    TInt SetDestSampleRate(const TInt aSampleRate);

    TInt GetDestChannelCount();
    TInt GetDestSampleRate();

    TInt SetDestBitRate(const TUint aBitRate);
    TInt GetDestBitRate();

    // aKind is a TAudioPlayerDestinationValueKind.
    void GetSupportedDestValuesL(const TInt aKind, RArray<TUint> &aValues);

    TInt SetDestContainerFormat(const TUint32 aUid);
    TInt GetDestContainerFormat(TUint32 &aUid);
};

struct CMMFMdaAudioToneUtility : public CActive {
private:
    MMdaAudioToneObserver &iObserver;
    TMdaAudioToneUtilityState iState;
    CMMFMdaAudioOpenComplete iOpener;

    TAny *iDispatchInstance;

public:
    CMMFMdaAudioToneUtility(MMdaAudioToneObserver &aObserver, const TInt aPriority, const TMdaPriorityPreference aPref);
    void ConstructL();

    ~CMMFMdaAudioToneUtility();

    static CMMFMdaAudioToneUtility *NewL(MMdaAudioToneObserver &aObserver, const TInt aPriority, const TMdaPriorityPreference aPref);

    TInt SetVolume(const TInt aNewVolume);
    TInt GetVolume();
    TInt MaxVolume();
    
    TInt GetBalance();
    TInt SetBalance(const TInt aBalance);

    void PrepareToPlayFileSequence(const TDesC &aFileName);
    void PrepareToPlayBufferSequence(const TDesC8 &aBuf);
    void PrepareToPlayFixedSequence(const TInt aSequenceIndex);
    void PrepareToPlayTone(const TInt aFreq, const TTimeIntervalMicroSeconds &aDuration);
    void PrepareToPlayDualTone(const TInt aFreqOne, const TInt aFreqTwo, const TTimeIntervalMicroSeconds &aDuration);
    void PrepareToPlayDTMFString(const TDesC &aDTMF);
    void CancelPrepare();

    void SetDTMFLengths(const TInt aToneOnMicros, const TInt aToneOffMicros, const TInt aPauseMicros);

    void Play();
    void CancelPlay();

    TInt FixedSequenceCount() const;
    void SetRepeats(const TInt aHowManyTimes, const TTimeIntervalMicroSeconds &aSilenceInterval);

    void CompletePrepare(TInt err);

    virtual void RunL();
    virtual void DoCancel();

    const TMdaAudioToneUtilityState State();
};

#endif
