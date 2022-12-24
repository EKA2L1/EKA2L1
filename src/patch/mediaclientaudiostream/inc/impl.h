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

#ifndef MEDIA_CLIENT_AUDIO_STREAM_IMPL_H_
#define MEDIA_CLIENT_AUDIO_STREAM_IMPL_H_

#include "main.h"

#include <e32base.h>
#include <e32std.h>

enum TMdaState {
    EMdaStatePlay = 0,
    EMdaStatePause = 1,
    EMdaStateStop = 2,
    EMdaStateWaitWorkThenStop = 3
};

struct TMMFMdaBufferNode {
    const TDesC8 *iBuffer;
    TDblQueLink iLink;

    TMMFMdaBufferNode()
        : iBuffer(NULL) {
    }
};

class CMMFMdaAudioStream;

struct CMMFMdaBufferQueue : public CActive {
    CMMFMdaAudioStream *iStream;
    TDblQue<TMMFMdaBufferNode> iBufferNodes;

    explicit CMMFMdaBufferQueue(CMMFMdaAudioStream *aStream);
    virtual ~CMMFMdaBufferQueue();

    virtual void FixupActiveStatus();
    virtual void CleanQueue();
    virtual void DoCancel();
};

struct CMMFMdaOutputBufferQueue : public CMMFMdaBufferQueue {
    TMMFMdaBufferNode *iCopied;

    explicit CMMFMdaOutputBufferQueue(CMMFMdaAudioStream *aStream);

    void WriteAndWait();
    void StartTransfer();

    virtual void CleanQueue();
    virtual void RunL();
};

struct CMMFMdaInputBufferQueue: public CMMFMdaBufferQueue {
    explicit CMMFMdaInputBufferQueue(CMMFMdaAudioStream *aStream);

    virtual void RunL();
    virtual void CleanQueue();

    void ReadAndWait();
};

class CMMFMdaOutputOpen : public CTimer {
protected:
    TBool iIsFixup;
    TBool iConstructed;
    CMMFMdaAudioStream *iParent;

public:
    explicit CMMFMdaOutputOpen();
    ~CMMFMdaOutputOpen();

    void FixupActiveStatus();

    virtual void RunL();

    void Open(CMMFMdaAudioStream *iStream);
    void DoCancel();
};

class CMMFMdaAudioStream {
protected:
    TAny *iDispatchInstance;

    TInt iPriority;
    TMdaPriorityPreference iPref;
    TMdaState iState;
    TBool iSetPriorityUnimplNotified;
    TBool iKeepOpenAtEnd;
    TTimeIntervalMicroSeconds iPosition;

    CMMFMdaOutputOpen iOpen;

public:
    explicit CMMFMdaAudioStream(const TInt aPriority, const TMdaPriorityPreference aPref);
    virtual ~CMMFMdaAudioStream();

    void RegisterNotifyBufferSent(TRequestStatus &aStatus);
    void CancelRegisterNotifyBufferSent();

    virtual void NotifyOpenComplete() = 0;

    void ConstructBaseL(TBool aIsIn);
    void StartRaw();

    virtual void Play();

    TInt RequestStop();

    TInt SetAudioPropertiesRaw(const TInt aFreq, const TInt aChannels);
    TInt SetAudioPropertiesWithMdaEnum(const TInt aFreq, const TInt aChannels);

    TInt SetBalance(const TInt aBalance);
    TInt GetBalance();

    TUint64 BytesRendered() const;
    const TTimeIntervalMicroSeconds &Position();

    TInt SetDataType(const TFourCC aFormat);
    TInt DataType(TFourCC &aFormat);

    TBool IsPriorityUnimplNotified() const;
    void SetPriorityUnimplNotified();
    
    TBool HasAlreadyPlay() const;
};

class CMMFMdaAudioOutputStream : public CMMFMdaAudioStream {
    friend struct CMMFMdaOutputBufferQueue;

    CMMFMdaOutputBufferQueue iBufferQueue;
    CPeriodic *iWaitBufferEndTimer;

public:
    MMdaAudioOutputStreamCallback &iCallback;

    CMMFMdaAudioOutputStream(MMdaAudioOutputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref);
    
    virtual ~CMMFMdaAudioOutputStream();
    virtual void NotifyOpenComplete();

    static CMMFMdaAudioOutputStream *NewL(MMdaAudioOutputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref);

    void Stop();
    void ConstructL();
    void DataWaitTimeout();
    void HandleBufferInsufficient();

    TInt Pause();
    TInt Resume();

    void WriteL(const TDesC8 &aData);
    void WriteWithQueueL(const TDesC8 &aData);

    TInt MaxVolume() const;
    TInt SetVolume(const TInt aNewVolume);
    TInt GetVolume() const;

    TInt KeepOpenAtEnd();
    void StartWaitBufferTimeoutTimer();
};

class CMMFMdaAudioInputStream: public CMMFMdaAudioStream {
private:
    friend struct CMMFMdaInputBufferQueue;
    CMMFMdaInputBufferQueue iBufferQueue;

public:
    MMdaAudioInputStreamCallback &iCallback;

    CMMFMdaAudioInputStream(MMdaAudioInputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref);
    
    virtual ~CMMFMdaAudioInputStream();
    virtual void NotifyOpenComplete();

    static CMMFMdaAudioInputStream *NewL(MMdaAudioInputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref);

    void Stop();
    void ConstructL();

    void ReadL(TDes8 &aData);
    void ReadWithQueueL(TDes8 &aData);
};

#endif
