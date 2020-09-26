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
    EMdaStateIdle = 0,
    EMdaStatePlay = 1,
    EMdaStatePause = 2,
    EMdaStateReady = 3,
    EMdaStateStop = 4
};

struct TMMFMdaBufferNode: public TDblQueLink {
    const TDesC8 *iBuffer;
};

struct CMMFMdaOutputBufferQueue: public CActive {
    CMMFMdaAudioOutputStream *iStream;
    TDblQue<TMMFMdaBufferNode> iBufferNodes;

    TMMFMdaBufferNode *iCopied;

    explicit CMMFMdaOutputBufferQueue(CMMFMdaAudioOutputStream *aStream);

    void WriteAndWait();
    ~CMMFMdaOutputBufferQueue();

    void FixupActiveStatus();

    void StartTransfer();
    void CleanQueue();

    virtual void RunL();
    virtual void DoCancel();
};

class CMMFMdaOutputOpen: public CIdle {
public:
    explicit CMMFMdaOutputOpen();
    
    void Open(CMMFMdaAudioOutputStream *iStream);
    void DoCancel();
};

class CMMFMdaAudioOutputStream {
    friend struct CMMFMdaOutputBufferQueue;

    TAny *iDispatchInstance;
    TInt iPriority;
    TMdaPriorityPreference iPref;
    TMdaState iState;
    TTimeIntervalMicroSeconds iPosition;

    CMMFMdaOutputBufferQueue iBufferQueue;
    CMMFMdaOutputOpen iOpen;

public:
    MMdaAudioOutputStreamCallback &iCallback;

    CMMFMdaAudioOutputStream(MMdaAudioOutputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref);
    ~CMMFMdaAudioOutputStream();

    void NotifyOpenComplete();

    static CMMFMdaAudioOutputStream *NewL(MMdaAudioOutputStreamCallback &aCallback, const TInt aPriority, const TMdaPriorityPreference aPref);
    void ConstructL();

    void StartRaw();

    void Play();
    void Stop();

    TInt Pause();
    TInt Resume();

    void WriteL(const TDesC8 &aData);
    void WriteWithQueueL(const TDesC8 &aData);

    TInt MaxVolume() const;
    TInt SetVolume(const TInt aNewVolume);
    TInt GetVolume() const;

    TInt SetAudioProperties(const TInt aFreq, const TInt aChannels);

    TInt SetBalance(const TInt aBalance);
    TInt GetBalance();

    TUint64 BytesRendered() const;
    const TTimeIntervalMicroSeconds &Position();

    TInt SetDataType(const TFourCC aFormat);
    TInt DataType(TFourCC &aFormat);

    void RegisterNotifyBufferSent(TRequestStatus &aStatus);
    void CancelRegisterNotifyBufferSent();
};

#endif
