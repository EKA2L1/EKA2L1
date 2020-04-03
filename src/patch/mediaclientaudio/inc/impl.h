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

#include <e32std.h>
#include <e32base.h>

enum TMdaState {
	EMdaStateIdle = 0,
	EMdaStatePlay = 1,
	EMdaStatePause = 2,
	EMdaStateReady = 3
};

class CMMFMdaAudioPlayerUtility: public CActive {
	TAny *iDispatchInstance;
	MMdaAudioPlayerCallback &iCallback;
	TInt iPriority;
	TMdaPriorityPreference iPref;
	TMdaState iState;
	TTimeIntervalMicroSeconds iDuration;
	
public:
	CMMFMdaAudioPlayerUtility(MMdaAudioPlayerCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref);
	~CMMFMdaAudioPlayerUtility();

	static CMMFMdaAudioPlayerUtility *NewL(MMdaAudioPlayerCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref);
	
	void ConstructL();

	virtual void RunL();
	virtual void DoCancel();
	
	void StartListeningForCompletion();
	void SupplyUrl(const TDesC &aUrl);
	
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

	void SetRepeats(const TInt aHowManyTimes, const TTimeIntervalMicroSeconds &aSilenceInterval);
};

#endif
