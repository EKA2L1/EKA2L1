/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#include <camimpl.h>
#include <dispatch.h>
#include <Log.h>

CCameraVideoChunkBufferBase::CCameraVideoChunkBufferBase()
    : iTotalSize(0) {    
}

CCameraVideoChunkBufferBase::~CCameraVideoChunkBufferBase() {
    iChunk.Close();
    iOffsets.Close();
}

void CCameraVideoChunkBufferBase::Construct() {
    iTotalSize = 0;
}

void CCameraVideoChunkBufferBase::PrepareRecieveL(TInt aTotalOfFrames, TUint32 aTotalSize) {
    if (iOffsets.Count() != aTotalOfFrames) {
        iOffsets.Reset();
        
        for (TInt i = 0; i < aTotalOfFrames; i++) {
            iOffsets.Append(0);
        }
    }
    
    if (iTotalSize < aTotalSize) {
        iChunk.Close();
        iChunk.CreateGlobal(_L(""), aTotalSize, aTotalSize, EOwnerProcess);
    }
    
    iTotalSize = aTotalSize;
}

TInt *CCameraVideoChunkBufferBase::GetOffsetArrays() const {
    return (TInt*)&iOffsets[0];
}

TDesC8 *CCameraVideoBuffer2Impl::DataL(TInt aFrameIndex) {
    TPtrC8 ptr;
    ptr.Set(ChunkL().Base() + ChunkOffsetL(aFrameIndex), FrameSize(aFrameIndex));
    
    // This is horrible, if game dead by this I will readjust.
    return &ptr;
}

CFbsBitmap& CCameraVideoBuffer2Impl::BitmapL(TInt aFrameIndex) {
    User::Leave(KErrNotSupported);
}

RChunk& CCameraVideoBuffer2Impl::ChunkL() {
    return iChunk;
}

TInt CCameraVideoBuffer2Impl::ChunkOffsetL(TInt aFrameIndex) {
    if ((aFrameIndex < 0) || (aFrameIndex <= iOffsets.Count())) {
        User::Leave(KErrArgument);
    }
    
    return iOffsets[aFrameIndex];
}

TInt CCameraVideoBuffer2Impl::FrameSize(TInt aFrameIndex) {
    if ((aFrameIndex < 0) || (aFrameIndex <= iOffsets.Count())) {
        User::Leave(KErrArgument);
    }
    
    if (aFrameIndex != iOffsets.Count() - 1) {
        return iOffsets[aFrameIndex + 1] - iOffsets[aFrameIndex];
    }
    
    return iTotalSize - iOffsets[aFrameIndex];
}

void CCameraVideoBuffer2Impl::Release() {
    iTotalSize = 0;
}

TDesC8 *CCameraVideoFramebuffer::DataL(TInt aFrameIndex) {
    TInt size = 0;
    if ((aFrameIndex < 0) || (aFrameIndex <= iOffsets.Count())) {
        User::Leave(KErrArgument);
    }
    
    if (aFrameIndex != iOffsets.Count() - 1) {
        size = iOffsets[aFrameIndex + 1] - iOffsets[aFrameIndex];
    } else {
        size = iTotalSize - iOffsets[aFrameIndex];
    }
    
    TPtrC8 ptr;
    ptr.Set(iChunk.Base() + iOffsets[aFrameIndex], size);
    
    // This is horrible, if game dead by this I will readjust.
    return &ptr;
}

CFbsBitmap *CCameraVideoFramebuffer::FrameL(TInt aFrameIndex) {
    User::Leave(KErrNotSupported);
    return NULL;
}

void CCameraVideoFramebuffer::Release() {
    iTotalSize = 0;
}

CCameraVideoCaptureObject::CCameraVideoCaptureObject()
    : CActive(EPriorityNormal)
    , iDispatch(NULL)
    , iObserver(NULL) {
    
}

CCameraVideoCaptureObject::~CCameraVideoCaptureObject() {
    ResetBufferArray();
}

void CCameraVideoCaptureObject::ResetBufferArray() {
    for (TInt i = 0; i < iBuffers.Count(); i++) {
        delete iBuffers[i];
    }

    iBuffers.Close();
}

void CCameraVideoCaptureObject::ConstructL(TAny *aDispatch, void *aObserver, TInt aObserverVersion) {
    iDispatch = aDispatch;
    iObserver = aObserver;

    iVersion = aObserverVersion;

    CActiveScheduler::Add(this);
}

void CCameraVideoCaptureObject::PrepareBuffersL(TInt aBuffers) {
    if (iBuffers.Count() != aBuffers) {
        ResetBufferArray();
        
        for (TInt i = 0; i < aBuffers; i++) {
            CCameraVideoChunkBufferBase *buffer = NULL;
            if (iVersion == 1) {
                buffer = new (ELeave) CCameraVideoFramebuffer;
            } else {
                buffer = new (ELeave) CCameraVideoBuffer2Impl;
            }
            
            buffer->Construct();
            iBuffers.Append(buffer);
        }
    }
}

TInt CCameraVideoCaptureObject::BuffersInUse() const {
    TInt count = 0;
    for (TInt i = 0; i < iBuffers.Count(); i++) {
        if (!iBuffers[i]->IsFree()) {
            count++;
        }
    }
    
    return count;
}

void CCameraVideoCaptureObject::StartCaptureL(TInt aSizeIndex, TInt aFrameRateIndex, CCamera::TFormat aFormat, TInt aFramePerBuffer, TRect *aClipRect) {
    if (IsActive()) {
        LogOut(KCameraCat, _L("The video capture is still taking place!"));
        User::Leave(KErrInUse);
    }

    iStatus = KRequestPending;
    TInt error = ECamTakeVideo(0, iDispatch, aSizeIndex, aFrameRateIndex, (TInt)aFormat, aFramePerBuffer, aClipRect, iStatus);
    
    if (error != KErrNone) {
        LogOut(KCameraCat, _L("Unable to request video capture!"));
        User::Leave(error);
    }

    SetActive();
}

CCameraVideoChunkBufferBase *CCameraVideoCaptureObject::PickFreeBuffer() {
    CCameraVideoChunkBufferBase *buffer = NULL;

    // Pick a free buffers
    for (TInt32 i = 0; i < iBuffers.Count(); i++) {
        if (iBuffers[i]->IsFree()) {
            buffer = iBuffers[i];
            break;
        }
    }

    return buffer;
}

void CCameraVideoCaptureObject::HandleCompleteV1() {
    CCameraVideoChunkBufferBase *buffer = PickFreeBuffer();
    MCameraObserver *observer = (MCameraObserver*)iObserver;

    if (!buffer) {
        observer->FrameBufferReady(NULL, KErrNoMemory);
        return;
    }
    
    TUint32 totalSize = 0;
    TInt error = ECamReceiveVideoBuffer(0, iDispatch, NULL, NULL, &totalSize);
    
    if (error != KErrNone) {
        observer->FrameBufferReady(NULL, error);
        return;
    }
    
    TRAP(error, buffer->PrepareRecieveL(iFramesPerBuffer, totalSize));
    if (error != KErrNone) {
        observer->FrameBufferReady(NULL, error);
        return;
    }
    
    error = ECamReceiveVideoBuffer(0, iDispatch, buffer->ChunkDataPtr(), buffer->GetOffsetArrays(), &totalSize);
    if (error != KErrNone) {
        observer->FrameBufferReady(NULL, error);
    }
    
    observer->FrameBufferReady((MFrameBuffer*)buffer, KErrNone);
}

void CCameraVideoCaptureObject::HandleCompleteV2() {
    CCameraVideoChunkBufferBase *buffer = PickFreeBuffer();
    MCameraObserver2 *observer = (MCameraObserver2*)iObserver;

    if (!buffer) {
        observer->VideoBufferReady((MCameraBuffer&)*buffer, KErrNoMemory);
        return;
    }
    
    TUint32 totalSize = 0;
    TInt error = ECamReceiveVideoBuffer(0, iDispatch, NULL, NULL, &totalSize);
    
    if (error != KErrNone) {
        observer->VideoBufferReady((MCameraBuffer&)*buffer, error);
        return;
    }
    
    TRAP(error, buffer->PrepareRecieveL(iFramesPerBuffer, totalSize));
    if (error != KErrNone) {
        observer->VideoBufferReady((MCameraBuffer&)*buffer, error);
        return;
    }
    
    error = ECamReceiveVideoBuffer(0, iDispatch, buffer->ChunkDataPtr(), buffer->GetOffsetArrays(), &totalSize);
    if (error != KErrNone) {
        observer->VideoBufferReady((MCameraBuffer&)*buffer, error);
    }

    observer->VideoBufferReady((MCameraBuffer&)*buffer, KErrNone);
}

void CCameraVideoCaptureObject::RunL() {
    if (iVersion == 1) {
        HandleCompleteV1();
    } else {
        HandleCompleteV2();
    }
}

void CCameraVideoCaptureObject::DoCancel() {
    if (ECamCancelTakeVideo(0, iDispatch) != KErrNone) {
        // Manually cancel it...
        TRequestStatus *status = &iStatus;
        User::RequestComplete(status, KErrCancel);
    }
}

void CCameraPlugin::PrepareVideoCaptureL(TFormat aFormat,TInt aSizeIndex,TInt aRateIndex,TInt aBuffersToUse,TInt aFramesPerBuffer) {
    if (iVideoCapture.IsActive()) {
        User::Leave(KErrInUse);
    }
    
    if (iInfo.iImageFormatsSupported & aFormat == 0) {
        User::Leave(KErrNotSupported);
    }
    
    if ((aSizeIndex < 0) || (aSizeIndex >= iInfo.iNumVideoFrameSizesSupported)) {
        User::Leave(KErrArgument);
    }

    if ((aRateIndex) || (aRateIndex >= iInfo.iNumVideoFrameRatesSupported)) {
        User::Leave(KErrArgument);
    }
    
    if ((aBuffersToUse < 0) || (aBuffersToUse > iInfo.iMaxBuffersSupported)) {
        User::Leave(KErrArgument);
    }
    
    if ((aFramesPerBuffer < 0) || (aFramesPerBuffer > iInfo.iMaxFramesPerBufferSupported)) {
        User::Leave(KErrArgument);
    }
    
    User::LeaveIfError(ECamQueryVideoFrameDimension(0, iDispatchInstance, aSizeIndex, aFormat, iVideoFrameDimCache));
    User::LeaveIfError(ECamQueryVideoFrameRate(0, iDispatchInstance, aRateIndex, aSizeIndex, aFormat, CCamera::EExposureAuto, &iVideoFrameRateCache));

    iVideoCapture.PrepareBuffersL(aBuffersToUse);

    iVideoCaptureNeedClip = EFalse;
    iVideoCaptureSizeIndex = aSizeIndex;
    iVideoCaptureFrameRateIndex = aRateIndex;
    iVideoCaptureFormat = aFormat;
    iVideoFramesPerBuffer = aFramesPerBuffer;
}

void CCameraPlugin::PrepareVideoCaptureL(TFormat aFormat,TInt aSizeIndex,TInt aRateIndex,TInt aBuffersToUse,TInt aFramesPerBuffer,const TRect& aClipRect) {
    PrepareVideoCaptureL(aFormat, aSizeIndex, aRateIndex, aBuffersToUse, aFramesPerBuffer);

    iVideoCaptureClipRect = aClipRect;
    iVideoCaptureNeedClip = ETrue;
}

void CCameraPlugin::StartVideoCapture() {
    if (iVideoCaptureSizeIndex < 0) {
        LogOut(KCameraCat, _L("Please prepare video capture first before calling capture!"));
        return;
    }
    
    if (iVideoCapture.IsActive()) {
        LogOut(KCameraCat, _L("Another image capture is taking place in this camera instance, please wait before calling capture again!"));
        return;
    }

    TRAPD(err, iVideoCapture.StartCaptureL(iVideoCaptureSizeIndex, iVideoCaptureFrameRateIndex, (CCamera::TFormat)iVideoCaptureFormat,
            iVideoFramesPerBuffer, iVideoCaptureNeedClip ? &iVideoCaptureClipRect : NULL));
 
    if (err != KErrNone) {
        LogOut(KCameraCat, _L("Fail to start capture with error code %d"), err);
        return;
    }
}

void CCameraPlugin::StopVideoCapture() {
    iVideoCapture.Cancel();
}

TBool CCameraPlugin::VideoCaptureActive() const {
    return iVideoCapture.IsActive();
}

void CCameraPlugin::EnumerateVideoFrameSizes(TSize& aSize,TInt aSizeIndex,TFormat aFormat) const {
    if (ECamQueryVideoFrameDimension(0, iDispatchInstance, aSizeIndex, aFormat, aSize) != KErrNone) {
        LogOut(KCameraCat, _L("Failed to enumerate video frame size!"));
    }
}

void CCameraPlugin::EnumerateVideoFrameRates(TReal32& aRate,TInt aRateIndex,CCamera::TFormat aFormat,TInt aSizeIndex,CCamera::TExposure aExposure) const {
    if (ECamQueryVideoFrameRate(0, iDispatchInstance, aRateIndex, aSizeIndex, aFormat, aExposure, &aRate) != KErrNone) {
        LogOut(KCameraCat, _L("Failed to enumerate video frame rate!"));
    }
}

void CCameraPlugin::GetFrameSize(TSize& aSize) const {
    aSize = iVideoFrameDimCache;
}

TReal32 CCameraPlugin::FrameRate() const {
    return iVideoFrameRateCache;
}

TInt CCameraPlugin::BuffersInUse() const {
    return iVideoCapture.BuffersInUse();
}

TInt CCameraPlugin::FramesPerBuffer() const {
    return iVideoCapture.FramesPerBuffer();
}
