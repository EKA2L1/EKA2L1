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

#include <e32err.h>
#include <e32std.h>

CCameraImageBufferImpl::CCameraImageBufferImpl()
    : iDataBuffer(NULL) {
}

void CCameraImageBufferImpl::ConstructL(TInt aDataSize) {
    iDataBuffer = HBufC8::NewL(aDataSize);
}

TDesC8 *CCameraImageBufferImpl::DataL(TInt aFrameIndex) {
    if (aFrameIndex != 0) {
        User::Leave(KErrArgument);
    }

    return &iDataBuffer->Des();
}

TUint8 *CCameraImageBufferImpl::DataPtr() {
    return (TUint8*)iDataBuffer->Ptr();
}

CFbsBitmap& CCameraImageBufferImpl::BitmapL(TInt aFrameIndex) {
    User::Leave(KErrNotSupported);
}

RChunk& CCameraImageBufferImpl::ChunkL() {
    User::Leave(KErrNotSupported);
}

TInt CCameraImageBufferImpl::ChunkOffsetL(TInt aFrameIndex) {
    User::Leave(KErrNotSupported);
    return 0;
}

TInt CCameraImageBufferImpl::FrameSize(TInt aFrameIndex) {
    if (aFrameIndex != 0) {
        User::Leave(KErrNotSupported);
    }
    return iDataBuffer->Length();
}

void CCameraImageBufferImpl::Release() {
    User::Free(iDataBuffer);
}

CCameraImageCaptureObject::CCameraImageCaptureObject()
    : CActive(EPriorityNormal)
    , iDispatch(NULL)
    , iObserver(NULL)
    , iDataBuffer(NULL) {
}

CCameraImageCaptureObject::~CCameraImageCaptureObject() {
    if (!iDataBuffer) {
        User::Free(iDataBuffer);
    }
    
    for (TInt i = 0; i < iImageBuffers.Count(); i++) {
        delete iImageBuffers[i];
    }

    iImageBuffers.Close();
    Deque();
}

void CCameraImageCaptureObject::ConstructL(TAny *aDispatch, void *aObserver, TInt aObserverVersion) {
    iDispatch = aDispatch;
    iObserver = aObserver;

    iObserverVersion = aObserverVersion;

    if (iObserverVersion > 1) {
        // Make a buffer array
        for (TInt32 i = 0; i < CCameraImageCaptureObject::CAMERA_MAX_IMAGE_BUFFER; i++) {
            CCameraImageBufferImpl *impl = new (ELeave) CCameraImageBufferImpl;
            impl->Clear();

            iImageBuffers.AppendL(impl);
        }
    }

    CActiveScheduler::Add(this);
}

TBool CCameraImageCaptureObject::StartCapture(TInt aIndex, CCamera::TFormat aFormat, TRect *aClipRect) {
    if (IsActive()) {
        LogOut(KCameraCat, _L("The capture is still taking place!"));
        return EFalse;
    }

    if (ECamTakeImage(0, iDispatch, aIndex, (TInt)aFormat, aClipRect, iStatus) != KErrNone) {
        LogOut(KCameraCat, _L("Unable to request still image capture!"));
        return EFalse;
    }

    SetActive();
    return ETrue;
}

void CCameraImageCaptureObject::HandleCompleteV1() {
    MCameraObserver *observer = (MCameraObserver*)iObserver;

    if (iStatus != KErrNone) {
        observer->ImageReady(NULL, NULL, iStatus.Int());
    } else {
        TInt imageBufferSize = 0;
        TInt error = ECamReceiveImage(0, iDispatch, &imageBufferSize, NULL);

        if (error != KErrNone) {
            LogOut(KCameraCat, _L("Failed to receive image data (image size can't be get)!"));
            observer->ImageReady(NULL, NULL, error);
        } else {
            if (!iDataBuffer) {
            	iDataBuffer = HBufC8::NewL(imageBufferSize);
            } else {
            	iDataBuffer = iDataBuffer->ReAllocL(imageBufferSize);
            }

            error = ECamReceiveImage(0, iDispatch, &imageBufferSize, iDataBuffer->Ptr());
            if (error != KErrNone) {
                LogOut(KCameraCat, _L("Failed to receive image data!"));
                observer->ImageReady(NULL, NULL, error);
            } else {  
                observer->ImageReady(NULL, iDataBuffer, KErrNone);
            }
        }
    }
}

void CCameraImageCaptureObject::HandleCompleteV2() {
	MCameraObserver2 *observer = (MCameraObserver2*)iObserver;
    CCameraImageBufferImpl *buffer = NULL;

    // Pick a free buffers
    for (TInt32 i = 0; i < iImageBuffers.Count(); i++) {
        if (iImageBuffers[i]->IsFree()) {
            buffer = iImageBuffers[i];
            break;
        }
    }

    if (buffer == NULL) {
        LogOut(KCameraCat, _L("Failed to get a free image camera buffer!"));

        // It still needs a reference :(
        observer->ImageBufferReady(*buffer, KErrNoMemory);
        return;
    }

    TInt imageBufferSize = 0;
    TInt error = ECamReceiveImage(0, iDispatch, &imageBufferSize, NULL);

    if (error != KErrNone) {
        LogOut(KCameraCat, _L("Failed to receive image data (image size can't be get)!"));
        observer->ImageBufferReady(*buffer, error);
    } else {
        TRAP(error, buffer->ConstructL(imageBufferSize));
        if (error != KErrNone) {
            observer->ImageBufferReady(*buffer, error);
            return;
        }

        error = ECamReceiveImage(0, iDispatch, &imageBufferSize, buffer->DataPtr());
        if (error != KErrNone) {
            LogOut(KCameraCat, _L("Failed to receive image data!"));
            observer->ImageBufferReady(*buffer, error);
        } else {  
            observer->ImageBufferReady(*buffer, KErrNone);
        }
    }
}

void CCameraImageCaptureObject::RunL() {
    if (iObserverVersion == 1) {
        HandleCompleteV1();
    } else {
        HandleCompleteV2();
    }
}

void CCameraImageCaptureObject::DoCancel() {
    if (ECamCancelTakeImage(0, iDispatch) != KErrNone) {
        // Manually cancel it...
    	TRequestStatus *status = &iStatus;
        User::RequestComplete(status, KErrCancel);
    }
}

void CCameraPlugin::PrepareImageCaptureL(TFormat aImageFormat,TInt aSizeIndex) {
    if (iImageCapture.IsActive()) {
        User::Leave(KErrInUse);
    }

    if (iInfo.iImageFormatsSupported & aImageFormat == 0) {
        User::Leave(KErrNotSupported);
    }

    if ((aSizeIndex < 0) || (iInfo.iNumImageSizesSupported <= aSizeIndex)) {
        User::Leave(KErrArgument);
    }

    TSize targetImageSize;
    if (ECamQueryStillImageSize(0, iDispatchInstance, aImageFormat, aSizeIndex, targetImageSize) != KErrNone) {
        User::Leave(KErrNotSupported);
    }

    iImageCaptureNeedClip = EFalse;
    iImageCaptureSizeIndex = aSizeIndex;
    iImageCaptureFormat = aImageFormat;
}

void CCameraPlugin::PrepareImageCaptureL(TFormat aImageFormat,TInt aSizeIndex,const TRect& aClipRect) {
    PrepareImageCaptureL(aImageFormat, aSizeIndex);

    iImageCaptureNeedClip = ETrue;
    iImageCaptureClipRect = aClipRect;
}

void CCameraPlugin::CaptureImage() {
    if (iImageCaptureSizeIndex < 0) {
        LogOut(KCameraCat, _L("Please prepare image capture first before calling capture!"));
        return;
    }
    
    if (iImageCapture.IsActive()) {
        LogOut(KCameraCat, _L("Another image capture is taking place in this camera instance, please wait before calling capture again!"));
        return;
    }

    if (!iImageCapture.StartCapture(iImageCaptureSizeIndex, (CCamera::TFormat)iImageCaptureFormat, iImageCaptureNeedClip ? &iImageCaptureClipRect : NULL)) {
        return;
    }
}

void CCameraPlugin::CancelCaptureImage() {
    if (iImageCapture.IsActive()) {
        iImageCapture.Cancel();
    }
}

void CCameraPlugin::EnumerateCaptureSizes(TSize& aSize,TInt aSizeIndex,CCamera::TFormat aFormat) const {
    if (ECamQueryStillImageSize(0, iDispatchInstance, aFormat, aSizeIndex, aSize) != KErrNone) {
        LogOut(KCameraCat, _L("Failed to query image capture size!"));
    }
}
