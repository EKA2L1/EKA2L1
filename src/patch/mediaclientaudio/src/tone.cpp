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

#include "common.h"
#include "dispatch.h"
#include "impl.h"

#ifdef EKA2
#include <e32cmn.h>
#endif

#include <e32def.h>
#include <e32std.h>

EXPORT_C CMdaAudioToneUtility* CMdaAudioToneUtility::NewL(MMdaAudioToneObserver& aObserver, CMdaServer* aServer) {
    CMdaAudioToneUtility *util = new (ELeave) CMdaAudioToneUtility();
    CleanupStack::PushL(util);
    util->iProperties = CMMFMdaAudioToneUtility::NewL(aObserver, 0, EMdaPriorityPreferenceTimeAndQuality);
    CleanupStack::Pop(util);

    return util;    
}

EXPORT_C CMdaAudioToneUtility* CMdaAudioToneUtility::NewL(MMdaAudioToneObserver& aObserver, CMdaServer* aServer, TInt aPriority, TMdaPriorityPreference aPref) {
    CMdaAudioToneUtility *util = new (ELeave) CMdaAudioToneUtility();
    CleanupStack::PushL(util);
    util->iProperties = CMMFMdaAudioToneUtility::NewL(aObserver, aPriority, aPref);
    CleanupStack::Pop(util);

    return util;
}

CMdaAudioToneUtility::~CMdaAudioToneUtility() {
    delete iProperties;
}

TMdaAudioToneUtilityState CMdaAudioToneUtility::State() {
    return iProperties->State();
}

TInt CMdaAudioToneUtility::MaxVolume() {
    return iProperties->MaxVolume();
}

TInt CMdaAudioToneUtility::Volume() {
    return iProperties->GetVolume();
}

void CMdaAudioToneUtility::SetVolume(TInt aVolume) {
    TInt err = iProperties->SetVolume(aVolume);
    if (err < KErrNone) {
        LogOut(KMcaCat, _L("CMdaAudioToneUtility::SetVolume: Error %d"), err);
    }
}

void CMdaAudioToneUtility::SetPriority(TInt aPriority, TMdaPriorityPreference aPref) {
    LogOut(KMcaCat, _L("CMdaAudioToneUtility::SetPriority unimplemented!"));
}

void CMdaAudioToneUtility::SetDTMFLengths(TTimeIntervalMicroSeconds32 aToneLength, TTimeIntervalMicroSeconds32 aToneOffLength,
    TTimeIntervalMicroSeconds32 aPauseLength) {
    LogOut(KMcaCat, _L("CMdaAudioToneUtility::SetDTMFLengths unimplemented!"));
}

void CMdaAudioToneUtility::SetRepeats(TInt aRepeatNumberOfTimes, const TTimeIntervalMicroSeconds& aTrailingSilence) {
    if ((aRepeatNumberOfTimes < 0) && (aRepeatNumberOfTimes != KMdaRepeatForever)) {
        LogOut(KMcaCat, _L("Invalid repeat numbers %d, set repeats does not do anything"), aRepeatNumberOfTimes);
        return;
    }

    iProperties->SetRepeats(aRepeatNumberOfTimes, aTrailingSilence);
}

void CMdaAudioToneUtility::SetVolumeRamp(const TTimeIntervalMicroSeconds& aRampDuration) {
    LogOut(KMcaCat, _L("CMdaAudioToneUtility::SetVolumeRamp unimplemented!"));
}

TInt CMdaAudioToneUtility::FixedSequenceCount() {
    return iProperties->FixedSequenceCount();
}

const TDesC& CMdaAudioToneUtility::FixedSequenceName(TInt aSequenceNumber) {
    LogOut(KMcaCat, _L("CMdaAudioToneUtility::FixedSequenceName unimplemented!"));
    return KNullDesC;
}

void CMdaAudioToneUtility::PrepareToPlayTone(TInt aFrequency, const TTimeIntervalMicroSeconds& aDuration) {
    iProperties->PrepareToPlayTone(aFrequency, aDuration);
}

void CMdaAudioToneUtility::PrepareToPlayDTMFString(const TDesC& aDTMF) {
    LogOut(KMcaCat, _L("CMdaAudioToneUtility::PrepareToPlayDTMFString unimplemented!"));
}

void CMdaAudioToneUtility::PrepareToPlayDesSequence(const TDesC8& aSequence) {
    iProperties->PrepareToPlayBufferSequence(aSequence);
}

void CMdaAudioToneUtility::PrepareToPlayFileSequence(const TDesC& aFileName) {
    iProperties->PrepareToPlayFileSequence(aFileName);
}

void CMdaAudioToneUtility::PrepareToPlayFixedSequence(TInt aSequenceNumber) {
    iProperties->PrepareToPlayFixedSequence(aSequenceNumber);
}

void CMdaAudioToneUtility::CancelPrepare() {
    iProperties->CancelPrepare();
}

void CMdaAudioToneUtility::Play() {
    iProperties->Play();
}

void CMdaAudioToneUtility::CancelPlay() {
    iProperties->CancelPlay();
}

EXPORT_C void CMdaAudioToneUtility::SetBalanceL(TInt aBalance) {
    User::LeaveIfError(iProperties->SetBalance(aBalance));
}

EXPORT_C TInt CMdaAudioToneUtility::GetBalanceL() {
    TInt err = iProperties->GetBalance();
    User::LeaveIfError(err);

    return err;
}

EXPORT_C void CMdaAudioToneUtility::PrepareToPlayDualTone(TInt aFrequencyOne, TInt aFrequencyTwo, const TTimeIntervalMicroSeconds& aDuration) {
    LogOut(KMcaCat, _L("CMdaAudioToneUtility::PrepareToPlayDualTone unimplemented!"));
}

#if (MCA_NEW >= 3)
EXPORT_C void CMdaAudioToneUtility::PrepareToPlayFileSequence(RFile& aFile) {
    LogOut(KMcaCat, _L("CMdaAudioToneUtility::PrepareToPlayFileSequence with file handle unimplemented!"));
}

EXPORT_C TAny* CMdaAudioToneUtility::CustomInterface(TUid aInterfaceId) {
    LogOut(KMcaCat, _L("CMdaAudioToneUtility::CustomInterface unimplemented!"));
    return NULL;
}
#endif