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

#include <fbs.h>
#include <w32std.h>

CCameraViewfinderBitmapObject::CCameraViewfinderBitmapObject()
    : CActive(CActive::EPriorityStandard)
    , iDispatch(NULL)
    , iObserver(NULL)
    , iObserverVersion(-1)
    , iDataBitmap(NULL)
    , iScreenDisplayMode(0) {
    CActiveScheduler::Add(this);
}

CCameraViewfinderBitmapObject::~CCameraViewfinderBitmapObject() {
    if (iDataBitmap) {
        delete iDataBitmap;
    }

    CleanBuffers();
    Deque();
}

void CCameraViewfinderBitmapObject::ConstructL(TAny *aDispatch, void *aObserver, TInt aObserverVersion) {
    RWsSession wsSession;
    User::LeaveIfError(wsSession.Connect());

    CleanupClosePushL(wsSession);

    CWsScreenDevice screenDevice(wsSession);
    User::LeaveIfError(screenDevice.Construct());

    iScreenDisplayMode = screenDevice.DisplayMode();

    CleanupStack::Pop();

    iDispatch = aDispatch;
    iObserver = aObserver;
    iObserverVersion = aObserverVersion;
}

void CCameraViewfinderBitmapObject::CleanBuffers() {
    for (TInt i = 0; i < iBitmapBuffers.Count(); i++) {
        delete iBitmapBuffers[i];
    }

    iBitmapBuffers.Close();
}

void CCameraViewfinderBitmapObject::StartL(TSize &aSize) {
    if (IsActive()) {
        LogOut(KCameraCat, _L("Bitmap viewfinder is already active!"));
        return;
    }
    iBitmapSize = aSize;
    if (iObserverVersion > 1) {
        if (iDataBitmap != NULL) {
            delete iDataBitmap;
            iDataBitmap = NULL;
        }
        // Make a buffer array
        for (TInt32 i = 0; i < CAMERA_MAX_BITMAP_BUFFER; i++) {
            CCameraImageBufferImpl *impl = new (ELeave) CCameraImageBufferImpl;
            impl->Clear();

            iBitmapBuffers.AppendL(impl);
        }
    } else {
        if (iDataBitmap && ((aSize != iDataBitmap->Header().iSizeInPixels))) {
            delete iDataBitmap;
            iDataBitmap = NULL;
        }

        if (!iDataBitmap) {
            iDataBitmap = new (ELeave) CFbsBitmap;
            User::LeaveIfError(iDataBitmap->Create(aSize, (TDisplayMode)iScreenDisplayMode));
        }
    }

    iStatus = KRequestPending;
    User::LeaveIfError(ECamStartViewfinder(0, iDispatch, &aSize, iScreenDisplayMode, &iStatus));

    SetActive();
}

void CCameraViewfinderBitmapObject::HandleCompleteV1() {
    MCameraObserver *observer = (MCameraObserver*)iObserver;

    if (iStatus == KErrNone) {
        TInt imageBufferSize = 0;
        TInt error = ECamReceiveImage(0, iDispatch, &imageBufferSize, NULL);

        if (error != KErrNone) {
            LogOut(KCameraCat, _L("Failed to receive viewfinder frame data (image size can't be get)!"));
        } else {
            iDataBitmap->BeginDataAccess();
            error = ECamReceiveImage(0, iDispatch, &imageBufferSize, (TUint8*)iDataBitmap->DataAddress());
            iDataBitmap->EndDataAccess();

            if (error != KErrNone) {
                LogOut(KCameraCat, _L("Failed to receive viewfinder frame data!"));
            } else {
                observer->ViewFinderFrameReady(*iDataBitmap);
            }
        }
    }
}

void CCameraViewfinderBitmapObject::HandleCompleteV2() {
    MCameraObserver2 *observer = (MCameraObserver2*)iObserver;
    CCameraImageBufferImpl *buffer = NULL;

    // Pick a free buffers
    for (TInt32 i = 0; i < iBitmapBuffers.Count(); i++) {
        if (iBitmapBuffers[i]->IsFree()) {
            buffer = iBitmapBuffers[i];
            break;
        }
    }

    if (buffer == NULL) {
        LogOut(KCameraCat, _L("Failed to get a free bitmap camera buffer!"));

        // It still needs a reference :(
        observer->ViewFinderReady(*buffer, KErrNoMemory);
        return;
    }

    TInt imageBufferSize = 0;
    TInt error = ECamReceiveImage(0, iDispatch, &imageBufferSize, NULL);

    if (error != KErrNone) {
        LogOut(KCameraCat, _L("Failed to receive image data (image size can't be get)!"));
        observer->ViewFinderReady(*buffer, error);
    } else {
        TRAP(error, buffer->ConstructBitmapL(iBitmapSize, iScreenDisplayMode));
        if (error != KErrNone) {
            observer->ViewFinderReady(*buffer, error);
            return;
        }

        error = ECamReceiveImage(0, iDispatch, &imageBufferSize, buffer->DataPtr());
        buffer->EndDataAccess();

        if (error != KErrNone) {
            LogOut(KCameraCat, _L("Failed to receive viewfinder bitmap data!"));
            observer->ViewFinderReady(*buffer, error);
        } else {
            observer->ViewFinderReady(*buffer, KErrNone);
        }
    }
}

void CCameraViewfinderBitmapObject::RunL() {
    if (iObserverVersion == 1) {
        HandleCompleteV1();
    } else {
        HandleCompleteV2();
    }

    iStatus = KRequestPending;
    User::LeaveIfError(ECamNextViewfinderFrame(0, iDispatch, &iStatus));

    SetActive();
}

void CCameraViewfinderBitmapObject::DoCancel() {
    ECamStopViewfinder(0, iDispatch);
}

void CCameraPlugin::StartViewFinderDirectL(RWsSession& aWs,CWsScreenDevice& aScreenDevice,RWindowBase& aWindow,TRect& aScreenRect) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StartViewFinderDirectL)"));
    //User::Leave(KErrNotSupported);
}

void CCameraPlugin::StartViewFinderDirectL(RWsSession& aWs,CWsScreenDevice& aScreenDevice,RWindowBase& aWindow,TRect& aScreenRect,TRect& aClipRect) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StartViewFinderDirectL with clip rect)"));
    //User::Leave(KErrNotSupported);
}

void CCameraPlugin::StartViewFinderBitmapsL(TSize& aSize) {
    iViewfinderBitmapCapture.StartL(aSize);
}

void CCameraPlugin::StartViewFinderBitmapsL(TSize& aSize,TRect& aClipRect) {
    LogOut(KCameraCat, _L("View finder bitmap with clip functions are currently routed to no clip!"));
    StartViewFinderBitmapsL(aSize);
    //User::Leave(KErrNotSupported);
}

void CCameraPlugin::StartViewFinderL(TFormat aImageFormat,TSize& aSize) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StartViewFinderL)"));
    User::Leave(KErrNotSupported);
}

void CCameraPlugin::StartViewFinderL(TFormat aImageFormat,TSize& aSize,TRect& aClipRect) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called StartViewFinderL)"));
    User::Leave(KErrNotSupported);
}

void CCameraPlugin::StopViewFinder() {
    iViewfinderBitmapCapture.Cancel();
}

TBool CCameraPlugin::ViewFinderActive() const {
    return iViewfinderBitmapCapture.IsActive();
}

void CCameraPlugin::SetViewFinderMirrorL(TBool aMirror) {
    LogOut(KCameraCat, _L("View finder functions are currently unsupported! (called SetViewFinderMirrorL)"));
}

TBool CCameraPlugin::ViewFinderMirror() const {
    return EFalse;
}
