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
#include <SoftInt.h>
#include <mda/common/audio.h>

#include "dispatch.h"
#include "impl.h"

#ifdef EKA2
#include <e32cmn.h>
#endif

#include <e32std.h>

static const TUint32 KWaitBufferTimeInMicroseconds = 100000;

// This sits between the redraw priority (50) and ws events priority (100) of the UI framework.
// Audio is intensive, we don't want redraw too take two much time, but at same time, we also
// want input or other events to be responsive and not missing out any events.
// Only apply to EKA2 onwards
static const TInt KMMFMdaOutputBufferPriority = 70;

static TInt OnWaitBufferTimeout(void *aUserdata) {
    CMMFMdaAudioOutputStream *stream = reinterpret_cast<CMMFMdaAudioOutputStream *>(aUserdata);
    if (stream) {
        stream->DataWaitTimeout();
    }

    return KErrNone;
}

CMMFMdaBufferQueue::CMMFMdaBufferQueue(CMMFMdaAudioStream *aStream)
#ifdef EKA2
    : CActive(KMMFMdaOutputBufferPriority)
#else
    : CActive(CActive::EPriorityStandard)
#endif
    , iStream(aStream) {
}

CMMFMdaBufferQueue::~CMMFMdaBufferQueue() {
    Deque();
}

void CMMFMdaBufferQueue::FixupActiveStatus() {
    if (IsActive()) {
        iStream->RegisterNotifyBufferSent(iStatus);
        Cancel();
    }
}

void CMMFMdaBufferQueue::CleanQueue() {
    // Flush all stored buffers
    while (!iBufferNodes.IsEmpty()) {
        TMMFMdaBufferNode *node = iBufferNodes.First();
        node->Deque();

        delete node;
    }
}

void CMMFMdaBufferQueue::DoCancel() {
    iStream->CancelRegisterNotifyBufferSent();
}

CMMFMdaOutputBufferQueue::CMMFMdaOutputBufferQueue(CMMFMdaAudioStream *aStream)
    : CMMFMdaBufferQueue(aStream)
    , iCopied(NULL) {
}

void CMMFMdaOutputBufferQueue::WriteAndWait() {
    CMMFMdaAudioOutputStream *outputStream = (CMMFMdaAudioOutputStream*) iStream;

    if (iBufferNodes.IsEmpty()) {
        iCopied = NULL;
        iStatus = KErrNone;

        outputStream->HandleBufferInsufficient();

        return;
    }

    TMMFMdaBufferNode *node = iBufferNodes.First();

    iStatus = KRequestPending;
    SetActive();

    // Register the notifcation for the buffer we are gonna sent
    outputStream->RegisterNotifyBufferSent(iStatus);
    outputStream->WriteL(*node->iBuffer);

    iCopied = node;
}

void CMMFMdaOutputBufferQueue::RunL() {
    CMMFMdaAudioOutputStream *outputStream = (CMMFMdaAudioOutputStream*) iStream;

    // Notify that last buffer has been copied
    if (iCopied) {
        if (iStatus == KErrNone)
            outputStream->iCallback.MaoscBufferCopied(KErrNone, *iCopied->iBuffer);

        if (iCopied) {
            iCopied->Deque();
            delete iCopied;
        }
    }

    // Stream might been canceled in buffer copied
    if ((iStatus != KErrAbort) && ((outputStream->iState == EMdaStatePlay) || (outputStream->iState == EMdaStateWaitWorkThenStop))) {
        WriteAndWait();
    }
}

void CMMFMdaOutputBufferQueue::CleanQueue() {
    CMMFMdaAudioOutputStream *outputStream = (CMMFMdaAudioOutputStream*) iStream;

    if (iCopied)
        outputStream->iCallback.MaoscBufferCopied(KErrAbort, *iCopied->iBuffer);

    CMMFMdaBufferQueue::CleanQueue();
    iCopied = NULL;
}

void CMMFMdaOutputBufferQueue::StartTransfer() {
    WriteAndWait();
}

CMMFMdaOutputOpen::CMMFMdaOutputOpen()
    : CTimer(CActive::EPriorityStandard)
    , iIsFixup(EFalse)
    , iConstructed(EFalse)
    , iParent(NULL) {
}

CMMFMdaOutputOpen::~CMMFMdaOutputOpen() {
    iIsFixup = EFalse;
    Deque();
}

void CMMFMdaOutputOpen::RunL() {
    LogOut(KMcaCat, _L("Open complete"));
    iParent->NotifyOpenComplete();
}

void CMMFMdaOutputOpen::Open(CMMFMdaAudioStream *stream) {
    iParent = stream;

	if (!iConstructed) {
		TRAPD(result, ConstructL());
        if (result != KErrNone) {
            LogOut(KMcaCat, _L("Error happens during open active object construction (code=%d)!"), result);
        }

        iConstructed = ETrue;
    }

    // Give some time for the application to do other things
    // Case with Magic Broom, initializes more stuffs before Open is called!
    // If server opcode 2 is not called, Open will not be able to write initial play data.
    static const TUint32 DelayInitializeCompleteUs = 10000;
    After(DelayInitializeCompleteUs);
}

void CMMFMdaOutputOpen::DoCancel() {
    if (iIsFixup) {
        TRequestStatus *statusPointer = &iStatus;
        User::RequestComplete(statusPointer, KErrCancel);
    }
}

void CMMFMdaOutputOpen::FixupActiveStatus() {
    iIsFixup = ETrue;

    if (IsActive()) {
        Cancel();
    }

    iIsFixup = EFalse;
}

/// SHARED BASE STREAM
void CMMFMdaAudioStream::RegisterNotifyBufferSent(TRequestStatus &aStatus) {
    EAudioDspStreamNotifyBufferReady(0, iDispatchInstance, aStatus);
}

void CMMFMdaAudioStream::CancelRegisterNotifyBufferSent() {
    EAudioDspStreamNotifyBufferReadyCancel(0, iDispatchInstance);
}

void CMMFMdaAudioStream::StartRaw() {
    if (EAudioDspStreamStart(0, iDispatchInstance) != KErrNone) {
        LogOut(KMcaCat, _L("Failed to start audio output stream"));
    }
}

void CMMFMdaAudioStream::Play() {
    iOpen.Open(this);
}

CMMFMdaAudioStream::CMMFMdaAudioStream(const TInt aPriority, const TMdaPriorityPreference aPref)
    : iDispatchInstance(NULL)
    , iPriority(aPriority)
    , iPref(aPref)
    , iState(EMdaStateStop)
    , iOpen()
    , iSetPriorityUnimplNotified(EFalse)
    , iKeepOpenAtEnd(EFalse) {
}

CMMFMdaAudioStream::~CMMFMdaAudioStream() {
    iOpen.Cancel();
    EAudioDspStreamDestroy(0, iDispatchInstance);
}

void CMMFMdaAudioStream::ConstructBaseL(TBool aIsIn) {
    iDispatchInstance = aIsIn ? EAudioDspInStreamCreate(0, NULL) : EAudioDspOutStreamCreate(0, NULL);

    if (!iDispatchInstance) {
        User::Leave(KErrGeneral);
    }

    CActiveScheduler::Add(&iOpen);
    iOpen.FixupActiveStatus();
}

TBool CMMFMdaAudioStream::HasAlreadyPlay() const {
    return (iOpen.IsActive() || (iState != EMdaStateStop));
}

TInt CMMFMdaAudioStream::RequestStop() {
    if (!iKeepOpenAtEnd) {
        return KErrNotSupported;
    }

    if ((iState == EMdaStateStop) || (iState == EMdaStateWaitWorkThenStop)) {
        return KErrNotReady;
    }

    iState = EMdaStateWaitWorkThenStop;
    return KErrNone;
}

TInt CMMFMdaAudioStream::SetAudioPropertiesWithMdaEnum(const TInt aFreq, const TInt aChannels) {
    const TInt realFreq = ConvertFreqEnumToNumber(aFreq);
    const TInt numChannels = ConvertChannelEnumToNum(aChannels);

    if ((realFreq == -1) || (numChannels == -1)) {
        // Do nothing i suppose
        return KErrNone;
    }

    return SetAudioPropertiesRaw(realFreq, numChannels);
}

TInt CMMFMdaAudioStream::SetAudioPropertiesRaw(const TInt aFreq, const TInt aChannels) {
    return EAudioDspStreamSetProperties(0, iDispatchInstance, aFreq, aChannels);
}

TInt CMMFMdaAudioStream::SetBalance(const TInt aBalance) {
    return EAudioDspStreamSetBalance(0, iDispatchInstance, aBalance);
}

TInt CMMFMdaAudioStream::GetBalance() {
    return EAudioDspStreamGetBalance(0, iDispatchInstance);
}

TUint64 CMMFMdaAudioStream::BytesRendered() const {
    TUint64 rendered = 0;
    const TInt result = EAudioDspStreamBytesRendered(0, iDispatchInstance, rendered);

    if (result != KErrNone) {
        LogOut(KMcaCat, _L("ERR:: Unable to get number of bytes rendered!"));
    }

    return rendered;
}

const TTimeIntervalMicroSeconds &CMMFMdaAudioStream::Position() {
    TUint64 time = 0;
    const TInt result = EAudioDspStreamPosition(0, iDispatchInstance, time);

    if (result != KErrNone) {
        LogOut(KMcaCat, _L("ERR:: Unable to get stream position!"));
    }

#ifdef EKA2
    iPosition = TTimeIntervalMicroSeconds(time);
#else
    iPosition = TTimeIntervalMicroSeconds(MakeSoftwareInt64FromHardwareUint64(time));
#endif

    return iPosition;
}

TInt CMMFMdaAudioStream::SetDataType(TFourCC aFormat) {
    return EAudioDspStreamSetFormat(0, iDispatchInstance, aFormat.FourCC());
}

TInt CMMFMdaAudioStream::DataType(TFourCC &aFormat) {
    TUint32 cc = 0;
    const TInt result = EAudioDspStreamGetFormat(0, iDispatchInstance, cc);

    if (result != KErrNone) {
        return result;
    }

    aFormat.Set(cc);
    return KErrNone;
}

TBool CMMFMdaAudioStream::IsPriorityUnimplNotified() const {
    return iSetPriorityUnimplNotified;
}

void CMMFMdaAudioStream::SetPriorityUnimplNotified() {
    iSetPriorityUnimplNotified = ETrue;
}

/// AUDIO OUTPUT STREAM
CMMFMdaAudioOutputStream::CMMFMdaAudioOutputStream(MMdaAudioOutputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref)
    : CMMFMdaAudioStream(aPriority, aPref)
    , iBufferQueue(this)
    , iWaitBufferEndTimer(NULL)
    , iCallback(aCallback) {
}

CMMFMdaAudioOutputStream::~CMMFMdaAudioOutputStream() {
    iBufferQueue.Cancel();
    iWaitBufferEndTimer->Cancel();

    delete iWaitBufferEndTimer;
}

CMMFMdaAudioOutputStream *CMMFMdaAudioOutputStream::NewL(MMdaAudioOutputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref) {
    CMMFMdaAudioOutputStream *newStream = new (ELeave) CMMFMdaAudioOutputStream(aCallback, aPriority, aPref);
    CleanupStack::PushL(newStream);
    newStream->ConstructL();
    CleanupStack::Pop(newStream);

    return newStream;
}

void CMMFMdaAudioOutputStream::ConstructL() {
    ConstructBaseL(EFalse);
    
    iWaitBufferEndTimer = CPeriodic::NewL(CActive::EPriorityStandard);

    CActiveScheduler::Add(&iBufferQueue);
    iBufferQueue.FixupActiveStatus();
}

void CMMFMdaAudioOutputStream::NotifyOpenComplete() {
    iCallback.MaoscOpenComplete(KErrNone);
}

void CMMFMdaAudioOutputStream::Stop() {
    iWaitBufferEndTimer->Cancel();

    iBufferQueue.CleanQueue();
    iBufferQueue.Cancel();

    if (iState == EMdaStateStop)
        return;

    // Reset the stat first
    EAudioDspStreamResetStat(0, iDispatchInstance);
    TInt result = EAudioDspStreamStop(0, iDispatchInstance);

    if (result < KErrNone) {
        LogOut(KMcaCat, _L("Failed to stop audio output stream"));
    } else {
        iState = EMdaStateStop;

        // Cancel is the definite return code for MMF-based audio output stream, and MMF-based
        // audio stream are implemented from 7.0 onwards
        // Citing: https://www.25yearsofprogramming.com/c-4/output-streaming.html (they took it out of some book)
        // On S60v1, the words are very ambiguous. I don't have a phone sadly to verify what it return. But let's guess this.
        // KoF needs PlayComplete to be either not called or called with KErrNone,
        // else while free, the object will call Stop and then call free inneath PlayComplete.
        // Choose to call KErrNone because Ashen needs it to unstuck audio thread
        if (result == KErrNone) {
            iCallback.MaoscPlayComplete(KErrCancel);
        } else {
            iCallback.MaoscPlayComplete(KErrNone);
        }
    }
}

void CMMFMdaAudioOutputStream::WriteL(const TDesC8 &aData) {
    if (EAudioDspOutStreamWrite(0, iDispatchInstance, aData.Ptr(), aData.Size()) != KErrNone) {
        LogOut(KMcaCat, _L("Error sending buffer!"));
        return;
    }
}

void CMMFMdaAudioOutputStream::WriteWithQueueL(const TDesC8 &aData) {
    iWaitBufferEndTimer->Cancel();

    TMMFMdaBufferNode *node = new (ELeave) TMMFMdaBufferNode;
    node->iBuffer = &aData;

    TBool isFirst = iBufferQueue.iBufferNodes.IsEmpty();
    iBufferQueue.iBufferNodes.AddLast(*node);

    if (iState == EMdaStateStop) {
        iState = EMdaStatePlay;
        StartRaw();
    }

    if (isFirst) {
        iBufferQueue.StartTransfer();
    }
}

TInt CMMFMdaAudioOutputStream::MaxVolume() const {
    const TInt result = EAudioDspOutStreamMaxVolume(0, iDispatchInstance);

    if (result < 0) {
        LogOut(KMcaCat, _L("ERR:: Max volume returns error code %d"), result);
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
        LogOut(KMcaCat, _L("ERR:: Get volume returns error code %d"), result);
        return 0;
    }

    return result;
}

TInt CMMFMdaAudioOutputStream::Pause() {
    if (iState != EMdaStatePlay) {
        return KErrNotReady;
    }

    iState = EMdaStatePause;
    Stop();

    return KErrNone;
}

TInt CMMFMdaAudioOutputStream::Resume() {
    if (iState != EMdaStatePause) {
        return KErrNotReady;
    }

    iState = EMdaStatePlay;
    Play();

    return KErrNone;
}

void CMMFMdaAudioOutputStream::DataWaitTimeout() {
    iWaitBufferEndTimer->Cancel();

    iState = EMdaStateStop;
    iCallback.MaoscPlayComplete(KErrUnderflow);
}

void CMMFMdaAudioOutputStream::StartWaitBufferTimeoutTimer() {
    iWaitBufferEndTimer->Start(KWaitBufferTimeInMicroseconds, KWaitBufferTimeInMicroseconds, TCallBack(OnWaitBufferTimeout, this));
}

TInt CMMFMdaAudioOutputStream::KeepOpenAtEnd() {
    iKeepOpenAtEnd = ETrue;
    return KErrNone;
}

void CMMFMdaAudioOutputStream::HandleBufferInsufficient() {
    if (!iKeepOpenAtEnd) {
        // Wait for a new buffer in an interval
        StartWaitBufferTimeoutTimer();
    } else {
        if (iState == EMdaStateWaitWorkThenStop) {
            DataWaitTimeout();
        }
    }
}

/// INPUT STREAM BUFFER QUEUE
CMMFMdaInputBufferQueue::CMMFMdaInputBufferQueue(CMMFMdaAudioStream *aStream)
    : CMMFMdaBufferQueue(aStream) {

}

void CMMFMdaInputBufferQueue::ReadAndWait() {
    CMMFMdaAudioInputStream *inputStream = (CMMFMdaAudioInputStream*) iStream;

    if (iBufferNodes.IsEmpty()) {
        return;
    }

    iStatus = KRequestPending;
    SetActive();

    TMMFMdaBufferNode *node = iBufferNodes.First();

    // Register the notifcation for the buffer we are gonna receive
    inputStream->RegisterNotifyBufferSent(iStatus);
    inputStream->ReadL(const_cast<TDes8 &>(static_cast<const TDes8 &>(*node->iBuffer)));
}

void CMMFMdaInputBufferQueue::CleanQueue() {
    CMMFMdaAudioInputStream *inputStream = (CMMFMdaAudioInputStream*) iStream;

    if (!iBufferNodes.IsEmpty()) {
        inputStream->iCallback.MaiscBufferCopied(KErrAbort, *(iBufferNodes.First()->iBuffer));
    }

    CMMFMdaBufferQueue::CleanQueue();
}

void CMMFMdaInputBufferQueue::RunL() {
    CMMFMdaAudioInputStream *inputStream = (CMMFMdaAudioInputStream*) iStream;
    TMMFMdaBufferNode *node = iBufferNodes.First();

    static_cast<TDes8&>(const_cast<TDesC8&>(*node->iBuffer)).SetLength(node->iBuffer->Length());
    inputStream->iCallback.MaiscBufferCopied(iStatus.Int(), *node->iBuffer);

    node->Deque();
    delete node;

    ReadAndWait();
}

/// AUDIO INPUT STREAM
CMMFMdaAudioInputStream::CMMFMdaAudioInputStream(MMdaAudioInputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref)
    : CMMFMdaAudioStream(aPriority, aPref)
    , iBufferQueue(this)
    , iCallback(aCallback) {

}

CMMFMdaAudioInputStream::~CMMFMdaAudioInputStream() {

}

void CMMFMdaAudioInputStream::NotifyOpenComplete() {
    iCallback.MaiscOpenComplete(KErrNone);
}

CMMFMdaAudioInputStream *CMMFMdaAudioInputStream::NewL(MMdaAudioInputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref) {
    CMMFMdaAudioInputStream *newStream = new (ELeave) CMMFMdaAudioInputStream(aCallback, aPriority, aPref);
    CleanupStack::PushL(newStream);
    newStream->ConstructL();
    CleanupStack::Pop(newStream);

    return newStream;
}

void CMMFMdaAudioInputStream::Stop() {
    iBufferQueue.CleanQueue();
    iBufferQueue.Cancel();

    if (iState == EMdaStateStop)
        return;

    // Reset the stat first
    EAudioDspStreamResetStat(0, iDispatchInstance);
    TInt result = EAudioDspStreamStop(0, iDispatchInstance);

    if (result < KErrNone) {
        LogOut(KMcaCat, _L("Failed to stop audio input stream"));
    } else {
        iState = EMdaStateStop;

        // See comments in CMMFMdaAudioOutputStream::Stop
        if (result == KErrNone) {
            iCallback.MaiscRecordComplete(KErrCancel);
        } else {
            iCallback.MaiscRecordComplete(KErrNone);
        }
    }
}

void CMMFMdaAudioInputStream::ConstructL() {
    ConstructBaseL(ETrue);

    CActiveScheduler::Add(&iBufferQueue);
    iBufferQueue.FixupActiveStatus();
}

void CMMFMdaAudioInputStream::ReadL(TDes8 &aData) {
    if (EAudioDspInStreamRead(0, iDispatchInstance, aData.Ptr(), aData.MaxSize()) != KErrNone) {
        LogOut(KMcaCat, _L("Error setting input receive buffer!"));
        return;
    }
}

void CMMFMdaAudioInputStream::ReadWithQueueL(TDes8 &aData) {
    TMMFMdaBufferNode *node = new (ELeave) TMMFMdaBufferNode;
    node->iBuffer = &aData;

    TBool isFirst = iBufferQueue.iBufferNodes.IsEmpty();
    iBufferQueue.iBufferNodes.AddLast(*node);

    if (iState == EMdaStateStop) {
        iState = EMdaStatePlay;
        StartRaw();
    }

    if (isFirst) {
        iBufferQueue.ReadAndWait();
    }
}