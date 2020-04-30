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

#include <mda/common/audio.h>

#include <dispatch.h>
#include <impl.h>
#include <log.h>

#include <e32cmn.h>
#include <e32std.h>

CMMFMdaOutputBufferQueue::CMMFMdaOutputBufferQueue(CMMFMdaAudioOutputStream *aStream)
    : CActive(CActive::EPriorityStandard)
    , iStream(aStream)
    , iCopied(NULL) {

}

void CMMFMdaOutputBufferQueue::WriteAndWait() {
    if (iBufferNodes.IsEmpty()) {
        iStream->iCallback.MaoscPlayComplete(KErrUnderflow);
        return;
    }

    if (iCopied == NULL) {
        iStream->StartRaw();
    }

    TMMFMdaBufferNode *node = iBufferNodes.First();

    // Register the notifcation for the buffer we are gonna sent
    iStream->RegisterNotifyBufferSent(iStatus);
    iStream->WriteL(*node->iBuffer);

    iCopied = node;
    SetActive();
}

CMMFMdaOutputBufferQueue::~CMMFMdaOutputBufferQueue() {
    Deque();
}

void CMMFMdaOutputBufferQueue::RunL() {
    // Notify that last buffer has been copied
    if (iCopied) {
        if (iStatus == KErrNone)
            iStream->iCallback.MaoscBufferCopied(KErrNone, *iCopied->iBuffer);

        iCopied->Deque();
        delete iCopied;
    }

    if (iStatus != KErrAbort) {
        iStatus = KRequestPending;
        WriteAndWait();
    }
}

void CMMFMdaOutputBufferQueue::CleanQueue() {
    iStream->iCallback.MaoscBufferCopied(KErrAbort, *iCopied->iBuffer);

    // Flush all stored buffers
    while (!iBufferNodes.IsEmpty()) {
        TMMFMdaBufferNode *node = iBufferNodes.First();
        node->Deque();

        delete node;
    }

    iCopied = NULL;
}

void CMMFMdaOutputBufferQueue::DoCancel() {
    iStream->CancelRegisterNotifyBufferSent();
}

void CMMFMdaOutputBufferQueue::StartTransfer() {
    WriteAndWait();
}

CMMFMdaOutputOpen::CMMFMdaOutputOpen()
    : CIdle(CActive::EPriorityStandard) {
}

static TInt OpenCompleteCallback(void *aUserdata) {
    LogOut(MCA_CAT, _L("Open complete"));
   
    CMMFMdaAudioOutputStream *stream = reinterpret_cast<CMMFMdaAudioOutputStream*>(aUserdata);
    stream->iCallback.MaoscOpenComplete(KErrNone);

    return 0;
}

void CMMFMdaOutputOpen::Open(CMMFMdaAudioOutputStream *stream) {
    Start(TCallBack(OpenCompleteCallback, stream));
}

/// AUDIO OUTPUT STREAM
CMMFMdaAudioOutputStream::CMMFMdaAudioOutputStream(MMdaAudioOutputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref)
    : iCallback(aCallback)
    , iPriority(aPriority)
    , iPref(aPref)
    , iState(EMdaStateReady)
    , iBufferQueue(this)
    , iOpen() {
}

CMMFMdaAudioOutputStream::~CMMFMdaAudioOutputStream() {
    EAudioDspStreamDestroy(0, iDispatchInstance);
}

CMMFMdaAudioOutputStream *CMMFMdaAudioOutputStream::NewL(MMdaAudioOutputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref) {
    CMMFMdaAudioOutputStream *newStream = new (ELeave) CMMFMdaAudioOutputStream(aCallback, aPriority, aPref);
    CleanupStack::PushL(newStream);
    newStream->ConstructL();
    CleanupStack::Pop(newStream);

    return newStream;
}

void CMMFMdaAudioOutputStream::ConstructL() {
    iDispatchInstance = EAudioDspOutStreamCreate(0, NULL);

    if (!iDispatchInstance) {
        User::Leave(KErrGeneral);
    }

    CActiveScheduler::Add(&iBufferQueue);
    CActiveScheduler::Add(&iOpen);
}

void CMMFMdaAudioOutputStream::NotifyOpenComplete() {
    // TODO: Do something
}

void CMMFMdaAudioOutputStream::StartRaw() {
    iState = EMdaStatePlay;

    if (EAudioDspStreamStart(0, iDispatchInstance) != KErrNone) {
        LogOut(MCA_CAT, _L("Failed to start audio output stream"));
    }
}

void CMMFMdaAudioOutputStream::Play() {
    // Simulates that buffer has been written to server
    iOpen.Open(this);
}

void CMMFMdaAudioOutputStream::Stop() {
    if (iState != EMdaStatePlay)
        return;

    iBufferQueue.CleanQueue();
    iBufferQueue.Cancel();

    if (EAudioDspStreamStop(0, iDispatchInstance) != KErrNone) {
        LogOut(MCA_CAT, _L("Failed to stop audio output stream"));
    } else {
        iState = EMdaStateReady;
    }
}

void CMMFMdaAudioOutputStream::WriteL(const TDesC8 &aData) {
    if (EAudioDspOutStreamWrite(0, iDispatchInstance, aData.Ptr(), aData.Size()) != KErrNone) {
        LogOut(MCA_CAT, _L("Error sending buffer!"));
        return;
    }
}

void CMMFMdaAudioOutputStream::WriteWithQueueL(const TDesC8 &aData) {
    TMMFMdaBufferNode *node = new (ELeave) TMMFMdaBufferNode;
    node->iBuffer = &aData;

    TBool isFirst = iBufferQueue.iBufferNodes.IsEmpty();
    iBufferQueue.iBufferNodes.AddLast(*node);

    if (isFirst) {
        iBufferQueue.StartTransfer();
    }
}

TInt CMMFMdaAudioOutputStream::MaxVolume() const {
    const TInt result = EAudioDspOutStreamMaxVolume(0, iDispatchInstance);

    if (result < 0) {
        LogOut(MCA_CAT, _L("ERR:: Max volume returns error code %d"), result);
        return 0;
    }

    return result;
}

TInt CMMFMdaAudioOutputStream::SetVolume(const TInt aNewVolume) {
    return EAudioDspOutStreamSetVolume(0, iDispatchInstance, aNewVolume);
}

TInt CMMFMdaAudioOutputStream::GetVolume() const {
    const TInt result = EAudioDspOutStreamGetVolume(0, iDispatchInstance);

    if (result < 0) {
        LogOut(MCA_CAT, _L("ERR:: Get volume returns error code %d"), result);
        return 0;
    }

    return result;
}

static TInt ConvertFreqEnumToNumber(const TInt caps) {
    switch (caps) {
    case TMdaAudioDataSettings::ESampleRate8000Hz:
        return 8000;
    
    case TMdaAudioDataSettings::ESampleRate11025Hz:
        return 11025;

    case TMdaAudioDataSettings::ESampleRate12000Hz:
        return 12000;

    case TMdaAudioDataSettings::ESampleRate16000Hz:
        return 16000;

    case TMdaAudioDataSettings::ESampleRate22050Hz:
        return 22050;

    case TMdaAudioDataSettings::ESampleRate24000Hz:
        return 24000;

    case TMdaAudioDataSettings::ESampleRate32000Hz:
        return 32000;

    case TMdaAudioDataSettings::ESampleRate44100Hz:
        return 44100;

    case TMdaAudioDataSettings::ESampleRate48000Hz:
        return 48000;

    case TMdaAudioDataSettings::ESampleRate96000Hz:
        return 96000;

    case TMdaAudioDataSettings::ESampleRate64000Hz:
        return 64000;

    default:
        break;
    }

    return -1;
}

static TInt ConvertChannelEnumToNum(const TInt caps) {
    switch (caps) {
    case TMdaAudioDataSettings::EChannelsMono:
        return 1;

    case TMdaAudioDataSettings::EChannelsStereo:
        return 2;

    default:
        break;
    }

    return -1;
}

TInt CMMFMdaAudioOutputStream::SetAudioProperties(const TInt aFreq, const TInt aChannels) {
    const TInt realFreq = ConvertFreqEnumToNumber(aFreq);
    const TInt numChannels = ConvertChannelEnumToNum(aChannels);

    if ((realFreq == -1) || (numChannels == -1)) {
        // Do nothing i suppose
        return KErrNone;
    }

    return EAudioDspStreamSetProperties(0, iDispatchInstance, realFreq, numChannels);
}

TInt CMMFMdaAudioOutputStream::SetBalance(const TInt aBalance) {
    return EAudioDspStreamSetBalance(0, iDispatchInstance, aBalance);
}

TInt CMMFMdaAudioOutputStream::GetBalance() {
    return EAudioDspStreamGetBalance(0, iDispatchInstance);
}

TUint64 CMMFMdaAudioOutputStream::BytesRendered() const {
    TUint64 rendered = 0;
    const TInt result = EAudioDspStreamBytesRendered(0, iDispatchInstance, rendered);

    if (result != KErrNone) {
        LogOut(MCA_CAT, _L("ERR:: Unable to get number of bytes rendered!"));
    }

    return rendered;
}

const TTimeIntervalMicroSeconds &CMMFMdaAudioOutputStream::Position() {
    TUint64 time = 0;
    const TInt result = EAudioDspStreamPosition(0, iDispatchInstance, time);

    if (result != KErrNone) {
        LogOut(MCA_CAT, _L("ERR:: Unable to get stream position!"));
    }

    iPosition = TTimeIntervalMicroSeconds(time);
    return iPosition;
}

TInt CMMFMdaAudioOutputStream::Pause() {
    if (iState != EMdaStatePlay) {
        return KErrNotReady;
    }

    iState = EMdaStateIdle;
    Stop();

    return KErrNone;
}

TInt CMMFMdaAudioOutputStream::Resume() {
    if (iState != EMdaStateIdle) {
        return KErrNotReady;
    }

    iState = EMdaStatePlay;
    Play();

    return KErrNone;
}

TInt CMMFMdaAudioOutputStream::SetDataType(TFourCC aFormat) {
    return EAudioDspStreamSetFormat(0, iDispatchInstance, aFormat.FourCC());
}

TInt CMMFMdaAudioOutputStream::DataType(TFourCC &aFormat) {
    TUint32 cc = 0;
    const TInt result = EAudioDspStreamGetFormat(0, iDispatchInstance, cc);

    if (result != KErrNone) {
        return result;
    }

    aFormat.Set(cc);
    return KErrNone;
}

void CMMFMdaAudioOutputStream::RegisterNotifyBufferSent(TRequestStatus &aStatus) {
    EAudioDspStreamNotifyBufferSentToDriver(0, iDispatchInstance, aStatus);
}

void CMMFMdaAudioOutputStream::CancelRegisterNotifyBufferSent() {
    EAudioDspStreamCancelNotifyBufferSentToDriver(0, iDispatchInstance);
}