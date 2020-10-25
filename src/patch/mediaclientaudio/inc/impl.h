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

#include <mdaaudiosampleplayer.h>
#include <mdaaudiosampleeditor.h>

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

struct CMMFMdaAudioOpenComplete: public CIdle {
public:
    CMMFMdaAudioOpenComplete();
    ~CMMFMdaAudioOpenComplete();
    
    void Open(CMMFMdaAudioUtility *aUtil);
    void FixupActiveStatus();

    virtual void DoCancel();
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

public:
    CMMFMdaAudioUtility(const TInt aPriority, const TMdaPriorityPreference aPref);
    ~CMMFMdaAudioUtility();

    void TransitionState(const TMdaState aNewState, const TInt aError = KErrNone);
    void ConstructL(const TUint32 aInitFlags);

    virtual void RunL();
    virtual void DoCancel();

    void StartListeningForCompletion();

    void SupplyUrl(const TDesC &aUrl);
    void SupplyData(TDesC8 &aData);

    void Play();
    void Stop();

    TInt Pause();

    TTimeIntervalMicroSeconds CurrentPosition();
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
    
    const TMdaState State() const {
    	return iState;
    }

    void SetRepeats(const TInt aHowManyTimes, const TTimeIntervalMicroSeconds &aSilenceInterval);
    
    // Base hook for devired
    virtual void OnStateChanged(const TMdaState aCurrentState, const TMdaState aPreviousState, const TInt aError) = 0;
};

struct CMMFMdaAudioPlayerUtility: public CMMFMdaAudioUtility {
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

    TInt SetDestContainerFormat(const TUint32 aUid);
    TInt GetDestContainerFormat(TUint32 &aUid);
};

#endif
