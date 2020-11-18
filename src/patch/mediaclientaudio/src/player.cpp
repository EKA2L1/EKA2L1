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

#include <mdaaudiosampleplayer.h>
#include <mda/common/resource.h>
#include <Log.h>

#include "common.h"
#include "dispatch.h"
#include "impl.h"

#include <e32std.h>

#if (MCA_NEW == 3)
CMdaAudioPlayerUtility::CMdaAudioPlayerUtility() {
}
#endif

CMdaAudioPlayerUtility::~CMdaAudioPlayerUtility() {
    delete iProperties;
}

EXPORT_C CMdaAudioPlayerUtility *CMdaAudioPlayerUtility::NewL(MMdaAudioPlayerCallback &aCallback, TInt aPriority, TMdaPriorityPreference aPref) {
    CMdaAudioPlayerUtility *util = new (ELeave) CMdaAudioPlayerUtility();
    CleanupStack::PushL(util);
    util->iProperties = CMMFMdaAudioPlayerUtility::NewL(aCallback, aPriority, aPref);
    CleanupStack::Pop(util);

    return util;
}

EXPORT_C CMdaAudioPlayerUtility *CMdaAudioPlayerUtility::NewFilePlayerL(const TDesC &aFileName,
    MMdaAudioPlayerCallback &aCallback, TInt aPriority, TMdaPriorityPreference aPref, CMdaServer *aServer) {
    CMdaAudioPlayerUtility *player = CMdaAudioPlayerUtility::NewL(aCallback, aPriority, aPref);
    player->OpenFileL(aFileName);

    return player;
}

EXPORT_C CMdaAudioPlayerUtility *CMdaAudioPlayerUtility::NewDesPlayerL(const TDesC8 &aData,
    MMdaAudioPlayerCallback &aCallback, TInt aPriority, TMdaPriorityPreference aPref, CMdaServer *aServer) {
    CMdaAudioPlayerUtility *player = CMdaAudioPlayerUtility::NewL(aCallback, aPriority, aPref);
    player->OpenDesL(aData);

    return player;
}

EXPORT_C CMdaAudioPlayerUtility *CMdaAudioPlayerUtility::NewDesPlayerReadOnlyL(const TDesC8 &aData, MMdaAudioPlayerCallback &aCallback,
    TInt aPriority, TMdaPriorityPreference aPref, CMdaServer *aServer) {
    CMdaAudioPlayerUtility *player = CMdaAudioPlayerUtility::NewL(aCallback, aPriority, aPref);
    player->OpenDesL(aData);

    return player;
}

/// == REPROXY ==
void CMdaAudioPlayerUtility::Play() {
    iProperties->Play();
}

void CMdaAudioPlayerUtility::Stop() {
    iProperties->Stop();
}

EXPORT_C TInt CMdaAudioPlayerUtility::Pause() {
    return iProperties->Pause();
}

EXPORT_C void CMdaAudioPlayerUtility::Close() {
    Stop();
}

#if (MCA_NEW == 3)
TInt CMdaAudioPlayerUtility::SetVolume(TInt aVolume) {
    return iProperties->SetVolume(aVolume);
}
#else
void CMdaAudioPlayerUtility::SetVolume(TInt aVolume) {
    iProperties->SetVolume(aVolume);
}
#endif

TInt CMdaAudioPlayerUtility::MaxVolume() {
    return iProperties->MaxVolume();
}

const TTimeIntervalMicroSeconds &CMdaAudioPlayerUtility::Duration() {
    return iProperties->Duration();
}

EXPORT_C void CMdaAudioPlayerUtility::OpenFileL(const TDesC &aFileName) {
    // It will auto detect filename anyway!
    iProperties->SupplyUrl(aFileName);
}

EXPORT_C void CMdaAudioPlayerUtility::OpenDesL(const TDesC8 &aDescriptor) {
    LogOut(KMcaCat, _L("Unimplemented function to open audio player stream with descriptor!"));
}

EXPORT_C void CMdaAudioPlayerUtility::OpenUrlL(const TDesC &aUrl, TInt aIapId, const TDesC8 &aMimeType) {
    // ID and MIME are ignored right now
    iProperties->SupplyUrl(aUrl);
}

EXPORT_C TInt CMdaAudioPlayerUtility::GetPosition(TTimeIntervalMicroSeconds &aPosition) {
    aPosition = iProperties->CurrentPosition();

    if (aPosition.Int64() < 0) {
#ifdef EKA2
        return aPosition.Int64();
#else
        return aPosition.Int64().GetTInt();
#endif
    }

    return KErrNone;
}

EXPORT_C TInt CMdaAudioPlayerUtility::GetVolume(TInt &aVolume) {
    aVolume = iProperties->GetVolume();

    if (aVolume < KErrNone) {
        return aVolume;
    }

    return KErrNone;
}

EXPORT_C void CMdaAudioPlayerUtility::SetPosition(const TTimeIntervalMicroSeconds &aPosition) {
    iProperties->SetCurrentPosition(aPosition);
}

EXPORT_C TInt CMdaAudioPlayerUtility::SetBalance(TInt aBalance) {
    return iProperties->SetBalance(aBalance);
}

EXPORT_C TInt CMdaAudioPlayerUtility::GetBalance(TInt &aBalance) {
    aBalance = iProperties->GetBalance();

    if (aBalance < KErrNone) {
        return aBalance;
    }

    return KErrNone;
}

#if (MCA_NEW == 3)
EXPORT_C TMMFDurationInfo CMdaAudioPlayerUtility::Duration(TTimeIntervalMicroSeconds &aDuration) {
    LogOut(KMcaCat, _L("Unimplemented function to get duration with state!"));
    return EMMFDurationInfoUnknown;
}

EXPORT_C void CMdaAudioPlayerUtility::OpenFileL(const RFile &aFile) {
    TBuf<256> fullname;
    aFile.FullName(fullname);

    OpenFileL(fullname);
}

EXPORT_C void CMdaAudioPlayerUtility::OpenFileL(const TMMSource &aSource) {
    LogOut(KMcaCat, _L("Unimplemented function to open file with MMsource!"));
}

EXPORT_C TInt CMdaAudioPlayerUtility::GetBitRate(TUint &aBitRate) {
    return iProperties->BitRate(aBitRate);
}
#endif

/// UNIMPLEMENT ALL THE WAY
#if (MCA_NEW >= 2)
EXPORT_C TInt CMdaAudioPlayerUtility::SetPriority(TInt aPriority, TMdaPriorityPreference aPref) {
    LogOut(KMcaCat, _L("Unimplemented function set priority!"));
    return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioPlayerUtility::GetNumberOfMetaDataEntries(TInt &aNumEntries) {
    LogOut(KMcaCat, _L("Unimplemented function get number of metadata entries!"));
    return KErrNotSupported;
}

EXPORT_C CMMFMetaDataEntry *CMdaAudioPlayerUtility::GetMetaDataEntryL(TInt aMetaDataIndex) {
    LogOut(KMcaCat, _L("Unimplemented function get metadata entry!"));
    return NULL;
}

EXPORT_C TInt CMdaAudioPlayerUtility::SetPlayWindow(const TTimeIntervalMicroSeconds &aStart,
    const TTimeIntervalMicroSeconds &aEnd) {
    LogOut(KMcaCat, _L("Unimplemented function set play window!"));
    return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioPlayerUtility::ClearPlayWindow() {
    LogOut(KMcaCat, _L("Unimplemented function clear play window!"));
    return KErrNotSupported;
}

EXPORT_C void CMdaAudioPlayerUtility::RegisterForAudioLoadingNotification(MAudioLoadingObserver &aCallback) {
    LogOut(KMcaCat, _L("Unimplemented function register audio loading nof!"));
}

EXPORT_C void CMdaAudioPlayerUtility::GetAudioLoadingProgressL(TInt &aPercentageComplete) {
    LogOut(KMcaCat, _L("Unimplemented function get audio loading progress!"));
}

EXPORT_C const CMMFControllerImplementationInformation &CMdaAudioPlayerUtility::ControllerImplementationInformationL() {
    LogOut(KMcaCat, _L("Unimplemented function get controller impl info!"));
    CMMFControllerImplementationInformation *info = NULL;
    return *info;
}

EXPORT_C TInt CMdaAudioPlayerUtility::CustomCommandSync(const TMMFMessageDestinationPckg &aDestination, TInt aFunction, const TDesC8 &aDataTo1, const TDesC8 &aDataTo2, TDes8 &aDataFrom) {
    LogOut(KMcaCat, _L("Custom command not supported!"));
    return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioPlayerUtility::CustomCommandSync(const TMMFMessageDestinationPckg &aDestination, TInt aFunction, const TDesC8 &aDataTo1, const TDesC8 &aDataTo2) {
    LogOut(KMcaCat, _L("Custom command not supported!"));
    return KErrNotSupported;
}

EXPORT_C void CMdaAudioPlayerUtility::CustomCommandAsync(const TMMFMessageDestinationPckg &aDestination, TInt aFunction, const TDesC8 &aDataTo1, const TDesC8 &aDataTo2, TDes8 &aDataFrom, TRequestStatus &aStatus) {
    LogOut(KMcaCat, _L("Custom command not supported!"));
}

EXPORT_C void CMdaAudioPlayerUtility::CustomCommandAsync(const TMMFMessageDestinationPckg &aDestination, TInt aFunction, const TDesC8 &aDataTo1, const TDesC8 &aDataTo2, TRequestStatus &aStatus) {
    LogOut(KMcaCat, _L("Custom command not supported!"));
}

void CMdaAudioPlayerUtility::SetRepeats(TInt aRepeatNumberOfTimes, const TTimeIntervalMicroSeconds &aTrailingSilence) {
    if ((aRepeatNumberOfTimes < 0) && (aRepeatNumberOfTimes != KMdaRepeatForever)) {
        LogOut(KMcaCat, _L("Invalid repeat numbers %d, set repeats does not do anything"), aRepeatNumberOfTimes);
        return;
    }

    iProperties->SetRepeats(aRepeatNumberOfTimes, aTrailingSilence);
}

void CMdaAudioPlayerUtility::SetVolumeRamp(const TTimeIntervalMicroSeconds &aRampDuration) {
    LogOut(KMcaCat, _L("Set volume ramp!"));
}
#endif

#if (MCA_NEW == 3)
EXPORT_C MMMFDRMCustomCommand *CMdaAudioPlayerUtility::GetDRMCustomCommand() {
    LogOut(KMcaCat, _L("Get DRM custom command not supported!"));
    return NULL;
}

EXPORT_C TInt CMdaAudioPlayerUtility::RegisterAudioResourceNotification(MMMFAudioResourceNotificationCallback &aCallback, TUid aNotificationEventUid, const TDesC8 &aNotificationRegistrationData) {
    LogOut(KMcaCat, _L("Register audio resource nof not supported!"));
    return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioPlayerUtility::CancelRegisterAudioResourceNotification(TUid aNotificationEventId) {
    LogOut(KMcaCat, _L("Cancel register audio resource nof not supported!"));
    return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioPlayerUtility::WillResumePlay() {
    LogOut(KMcaCat, _L("Will resume play unimplemented"));
    return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioPlayerUtility::SetThreadPriority(const TThreadPriority &aThreadPriority) const {
    LogOut(KMcaCat, _L("Set thread priority not supported!"));
    return KErrNotSupported;
}

EXPORT_C void CMdaAudioPlayerUtility::UseSharedHeap() {
    LogOut(KMcaCat, _L("Use shared heap for this player utility instance"));
}
#endif
/// == END REPROXY ==
