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

static TInt OnWaitBufferTimeout(void *aUserdata) {
    CMMFMdaAudioOutputStream *stream = reinterpret_cast<CMMFMdaAudioOutputStream *>(aUserdata);
    if (stream) {
        stream->DataWaitTimeout();
    }

    return KErrNone;
}

CMMFMdaOutputBufferQueue::CMMFMdaOutputBufferQueue(CMMFMdaAudioOutputStream *aStream)
    : CActive(CActive::EPriorityStandard)
    , iStream(aStream)
    , iCopied(NULL) {
}

void CMMFMdaOutputBufferQueue::WriteAndWait() {
    if (iBufferNodes.IsEmpty()) {
        iCopied = NULL;
        iStatus = KErrNone;

        iStream->HandleBufferInsufficient();

        return;
    }

    TMMFMdaBufferNode *node = iBufferNodes.First();

    iStatus = KRequestPending;
    SetActive();

    // Register the notifcation for the buffer we are gonna sent
    iStream->RegisterNotifyBufferSent(iStatus);
    iStream->WriteL(*node->iBuffer);

    iCopied = node;
}

CMMFMdaOutputBufferQueue::~CMMFMdaOutputBufferQueue() {
    Deque();
}

void CMMFMdaOutputBufferQueue::RunL() {
    // Notify that last buffer has been copied
    if (iCopied) {
        if (iStatus == KErrNone)
            iStream->iCallback.MaoscBufferCopied(KErrNone, *iCopied->iBuffer);

        if (iCopied) {
            iCopied->Deque();
            delete iCopied;
        }
    }

    // Stream might been canceled in buffer copied
    if ((iStatus != KErrAbort) && ((iStream->iState == EMdaStatePlay) || (iStream->iState == EMdaStateWaitWorkThenStop))) {
        WriteAndWait();
    }
}

void CMMFMdaOutputBufferQueue::CleanQueue() {
    if (iCopied)
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

void CMMFMdaOutputBufferQueue::FixupActiveStatus() {
    if (IsActive()) {
        iStream->RegisterNotifyBufferSent(iStatus);
        Cancel();
    }
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
    iParent->iCallback.MaoscOpenComplete(KErrNone);
}

void CMMFMdaOutputOpen::Open(CMMFMdaAudioOutputStream *stream) {
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

/// AUDIO OUTPUT STREAM
CMMFMdaAudioOutputStream::CMMFMdaAudioOutputStream(MMdaAudioOutputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref)
    : iPriority(aPriority)
    , iPref(aPref)
    , iState(EMdaStateStop)
    , iBufferQueue(this)
    , iOpen()
    , iSetPriorityUnimplNotified(EFalse)
    , iWaitBufferEndTimer(NULL)
    , iKeepOpenAtEnd(EFalse)
    , iCallback(aCallback) {
}

CMMFMdaAudioOutputStream::~CMMFMdaAudioOutputStream() {
    iOpen.Cancel();
    iBufferQueue.Cancel();
    iWaitBufferEndTimer->Cancel();

    EAudioDspStreamDestroy(0, iDispatchInstance);

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
    iDispatchInstance = EAudioDspOutStreamCreate(0, NULL);

    if (!iDispatchInstance) {
        User::Leave(KErrGeneral);
    }
    
    iWaitBufferEndTimer = CPeriodic::NewL(CActive::EPriorityStandard);

    CActiveScheduler::Add(&iBufferQueue);
    CActiveScheduler::Add(&iOpen);

    iOpen.FixupActiveStatus();
    iBufferQueue.FixupActiveStatus();
}

void CMMFMdaAudioOutputStream::NotifyOpenComplete() {
    // TODO: Do something
}

void CMMFMdaAudioOutputStream::StartRaw() {
    if (EAudioDspStreamStart(0, iDispatchInstance) != KErrNone) {
        LogOut(KMcaCat, _L("Failed to start audio output stream"));
    }
}

void CMMFMdaAudioOutputStream::Play() {
    iOpen.Open(this);
}

TBool CMMFMdaAudioOutputStream::HasAlreadyPlay() const {
    return (iOpen.IsActive() || (iState != EMdaStateStop));
}

void CMMFMdaAudioOutputStream::Stop() {
    iWaitBufferEndTimer->Cancel();

    iBufferQueue.CleanQueue();
    iBufferQueue.Cancel();

    if (iState == EMdaStateStop)
        return;

    // Reset the stat first
    EAudioDspStreamResetStat(0, iDispatchInstance);

    if (EAudioDspStreamStop(0, iDispatchInstance) != KErrNone) {
        LogOut(KMcaCat, _L("Failed to stop audio output stream"));
    } else {
        iState = EMdaStateStop;
        iCallback.MaoscPlayComplete(KErrCancel);
    }
}

TInt CMMFMdaAudioOutputStream::RequestStop() {
    if (!iKeepOpenAtEnd) {
        return KErrNotSupported;
    }

    if ((iState == EMdaStateStop) || (iState == EMdaStateWaitWorkThenStop)) {
        return KErrNotReady;
    }

    iState = EMdaStateWaitWorkThenStop;
    return KErrNone;
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

TInt CMMFMdaAudioOutputStream::SetAudioPropertiesWithMdaEnum(const TInt aFreq, const TInt aChannels) {
    const TInt realFreq = ConvertFreqEnumToNumber(aFreq);
    const TInt numChannels = ConvertChannelEnumToNum(aChannels);

    if ((realFreq == -1) || (numChannels == -1)) {
        // Do nothing i suppose
        return KErrNone;
    }

    return SetAudioPropertiesRaw(realFreq, numChannels);
}

TInt CMMFMdaAudioOutputStream::SetAudioPropertiesRaw(const TInt aFreq, const TInt aChannels) {
    return EAudioDspStreamSetProperties(0, iDispatchInstance, aFreq, aChannels);
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
        LogOut(KMcaCat, _L("ERR:: Unable to get number of bytes rendered!"));
    }

    return rendered;
}

const TTimeIntervalMicroSeconds &CMMFMdaAudioOutputStream::Position() {
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

TBool CMMFMdaAudioOutputStream::IsPriorityUnimplNotified() const {
    return iSetPriorityUnimplNotified;
}

void CMMFMdaAudioOutputStream::SetPriorityUnimplNotified() {
    iSetPriorityUnimplNotified = ETrue;
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