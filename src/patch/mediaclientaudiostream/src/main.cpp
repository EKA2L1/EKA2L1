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

#include <AudCommon.h>
#include <Log.h>
#include <mda/common/audio.h>
#include <mda/common/resource.h>

#include "dispatch.h"
#include "impl.h"
#include "main.h"

#include <e32std.h>

/// == AUDIO OUTPUT STREAM PROXY ==
CMdaAudioOutputStream *CMdaAudioOutputStream::NewL(MMdaAudioOutputStreamCallback &aCallBack, CMdaServer *aServer) {
    CMdaAudioOutputStream *stream = new (ELeave) CMdaAudioOutputStream();
    CleanupStack::PushL(stream);
    stream->iProperties = CMMFMdaAudioOutputStream::NewL(aCallBack, 0, EMdaPriorityPreferenceTimeAndQuality);
    CleanupStack::Pop(stream);

    return stream;
}

CMdaAudioOutputStream *CMdaAudioOutputStream::NewL(MMdaAudioOutputStreamCallback &aCallBack, TInt aPriority, TMdaPriorityPreference aPref) {
    CMdaAudioOutputStream *stream = new (ELeave) CMdaAudioOutputStream();
    CleanupStack::PushL(stream);
    stream->iProperties = CMMFMdaAudioOutputStream::NewL(aCallBack, aPriority, aPref);
    CleanupStack::Pop(stream);

    return stream;
}

CMdaAudioOutputStream::CMdaAudioOutputStream()
    : iProperties(NULL) {
}

void CMdaAudioOutputStream::Open(TMdaPackage *aPackage) {
    if (iProperties->HasAlreadyPlay()) {
        LogOut(KMcaCat, _L("WARN:: Stream has already been opened! This open call is ignored."));
        return;
    }

    TMdaAudioDataSettings *settings = reinterpret_cast<TMdaAudioDataSettings *>(aPackage);

    // Try to set audio properties
    TInt realFreq = 0;
    TInt numChannels = 0;

    if (settings) {
        realFreq = ConvertFreqEnumToNumber(settings->iSampleRate);
        numChannels = ConvertChannelEnumToNum(settings->iChannels);

        if (realFreq == -1) {
            realFreq = settings->iSampleRate;
        }

        if (numChannels == -1) {
            numChannels = settings->iChannels;
        }
    } else {
        // Default properties...
        realFreq = 8000;
        numChannels = 1;
    }

    const TInt result = iProperties->SetAudioPropertiesRaw(realFreq, numChannels);

    if (result != KErrNone) {
        LogOut(KMcaCat, _L("ERR:: Unable to set audio properties on stream opening!"));

        // Further error should not enable stream write.
        iProperties->iCallback.MaoscOpenComplete(result);
        return;
    }

    iProperties->NotifyOpenComplete();
    iProperties->Play();
}

void CMdaAudioOutputStream::SetAudioPropertiesL(TInt aSampleRate, TInt aChannels) {
    const TInt result = iProperties->SetAudioPropertiesWithMdaEnum(aSampleRate, aChannels);

    if (result != KErrNone) {
        LogOut(KMcaCat, _L("ERR:: Set audio properties failed!"));
        User::Leave(result);
    }
}

TInt CMdaAudioOutputStream::MaxVolume() {
    return iProperties->MaxVolume();
}

TInt CMdaAudioOutputStream::Volume() {
    return iProperties->GetVolume();
}

void CMdaAudioOutputStream::SetVolume(const TInt aNewVolume) {
    iProperties->SetVolume(aNewVolume);
}

void CMdaAudioOutputStream::SetPriority(TInt aPriority, TMdaPriorityPreference aPref) {
    if (iProperties->IsPriorityUnimplNotified()) {
        return;
    }

    LogOut(KMcaCat, _L("WARN:: Set priority unimplemented %d!"), aPriority);
    iProperties->SetPriorityUnimplNotified();
}

void CMdaAudioOutputStream::WriteL(const TDesC8 &aData) {
    iProperties->WriteWithQueueL(aData);
}

void CMdaAudioOutputStream::Stop() {
    iProperties->Stop();
}

const TTimeIntervalMicroSeconds &CMdaAudioOutputStream::Position() {
    return iProperties->Position();
}

CMdaAudioOutputStream::~CMdaAudioOutputStream() {
    delete iProperties;
}

TInt CMdaAudioOutputStream::Pause() {
    return iProperties->Pause();
}

TInt CMdaAudioOutputStream::Resume() {
    return iProperties->Resume();
}

void CMdaAudioOutputStream::SetBalanceL(TInt aBalance) {
    User::LeaveIfError(iProperties->SetBalance(aBalance));
}

TInt CMdaAudioOutputStream::GetBalanceL() const {
    const TInt balance = iProperties->GetBalance();
    User::LeaveIfError(balance);

    return balance;
}

TInt CMdaAudioOutputStream::GetBytes() {
    return static_cast<TInt>(iProperties->BytesRendered());
}

void CMdaAudioOutputStream::SetDataTypeL(TFourCC aAudioType) {
    User::LeaveIfError(iProperties->SetDataType(aAudioType));
}

TFourCC CMdaAudioOutputStream::DataType() const {
    TFourCC format = 0;
    const TInt result = iProperties->DataType(format);

    if (result != KErrNone) {
        LogOut(KMcaCat, _L("ERR:: Unable to get data type!"));
    }

    return format;
}

#ifdef MMF_BASE_CLIENT_UTILITY
TInt CMdaAudioOutputStream::RegisterAudioResourceNotification(MMMFAudioResourceNotificationCallback &aCallback, TUid aNotificationEventUid, const TDesC8 &aNotificationRegistrationData) {
    LogOut(KMcaCat, _L("WARN:: Register audio notification not supported!"));
    return KErrNotSupported;
}

TInt CMdaAudioOutputStream::CancelRegisterAudioResourceNotification(TUid aNotificationEventId) {
    LogOut(KMcaCat, _L("WARN:: Cancel register audio notification not supported!"));
    return KErrNotSupported;
}
#endif

TInt CMdaAudioOutputStream::WillResumePlay() {
    LogOut(KMcaCat, _L("WARN:: Will resume play not supported!"));
    return KErrNotSupported;
}

TAny *CMdaAudioOutputStream::CustomInterface(TUid aInterfaceId) {
    LogOut(KMcaCat, _L("WARN:: Custom interface not supported!"));
    return NULL;
}

// Note: Special functions

TInt CMdaAudioOutputStream::KeepOpenAtEnd() {
    return iProperties->KeepOpenAtEnd();
}

TInt CMdaAudioOutputStream::RequestStop() {
    return iProperties->RequestStop();
}

#ifndef EKA2
EXPORT_C TInt E32Dll(TDllReason reason) {
    return 0;
}
#endif