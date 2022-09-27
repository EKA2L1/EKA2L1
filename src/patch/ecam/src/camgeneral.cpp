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

EXPORT_C TInt CCamera::CamerasAvailable() {
    return ECamGetNumberOfCameras(0);
}

EXPORT_C TInt CCamera::CameraVersion() {
    return 0;
}

EXPORT_C CCamera::CCamera() {
}

CCameraPlugin::CCameraPlugin(MCameraObserver *aObserver) 
    : CCamera()
    , iDispatchInstance(NULL)
    , iObserver(aObserver)
    , iVersion(1)
    , iImageCaptureSizeIndex(-1)
    , iVideoCaptureSizeIndex(-1)
    , iJpegQuality(1) {
}

CCameraPlugin::CCameraPlugin(MCameraObserver2 *aObserver) 
    : CCamera()
    , iDispatchInstance(NULL)
    , iObserver(aObserver)
    , iVersion(2)
    , iImageCaptureSizeIndex(-1) 
    , iVideoCaptureSizeIndex(-1)
    , iJpegQuality(1) {
}

CCameraPlugin::~CCameraPlugin() {
    delete iEventCompleteIdle;
}

void CCameraPlugin::ConstructL(TInt aCameraIndex) {
    iDispatchInstance = ECamCreate(0, aCameraIndex, iInfo);
    User::LeaveIfNull(iDispatchInstance);

    iImageCapture.ConstructL(iDispatchInstance, iObserver, iVersion);
    iVideoCapture.ConstructL(iDispatchInstance, iObserver, iVersion);
    iViewfinderBitmapCapture.ConstructL(iDispatchInstance, iObserver, iVersion);

    iEventCompleteIdle = CIdle::NewL(CActive::EPriorityStandard);
}

void CCameraPlugin::ConstructSharedL(TAny *aSharedHandle) {
    iDispatchInstance = ECamDuplicate(0, aSharedHandle, iInfo);
    User::LeaveIfNull(iDispatchInstance);

    iImageCapture.ConstructL(iDispatchInstance, iObserver, iVersion);
    iVideoCapture.ConstructL(iDispatchInstance, iObserver, iVersion);

    iEventCompleteIdle = CIdle::NewL(CActive::EPriorityStandard);
}

void CCameraPlugin::CameraInfo(TCameraInfo &aInfo) const {
    aInfo = iInfo;
}

void CCameraPlugin::SetJpegQuality(TInt aQuality) {
    LogOut(KCameraCat, _L("Set JPEG quality is currently unsupported!"));
    iJpegQuality = aQuality;
}

TInt CCameraPlugin::JpegQuality() const {
    return iJpegQuality;
}

void CCameraPlugin::HandleReserveCompleteV1(TInt aError) {
    MCameraObserver *observer = (MCameraObserver*)iObserver;
    observer->ReserveComplete(aError);
}

void CCameraPlugin::HandleReserveCompleteV2(TInt aError) {
    TECAMEvent reserveEvent(KUidECamEventReserveComplete, aError);
    MCameraObserver2 *observer = (MCameraObserver2*)iObserver;
    observer->HandleEvent(reserveEvent);
}

void CCameraPlugin::HandlePowerOnCompleteV1(TInt aError) {
    MCameraObserver *observer = (MCameraObserver*)iObserver;
    observer->PowerOnComplete(aError);
}

void CCameraPlugin::HandlePowerOnCompleteV2(TInt aError) {
    TECAMEvent reserveEvent(KUidECamEventPowerOnComplete, aError);
    MCameraObserver2 *observer = (MCameraObserver2*)iObserver;
    observer->HandleEvent(reserveEvent);
}

void CCameraPlugin::HandleReserveComplete(TInt aError) {
    if (iVersion == 1) {
        HandleReserveCompleteV1(aError);
    } else {
        HandleReserveCompleteV2(aError);
    }
}

void CCameraPlugin::HandlePowerOnComplete(TInt aError) {
    if (iVersion == 1) {
        HandlePowerOnCompleteV1(aError);
    } else {
        HandlePowerOnCompleteV2(aError);
    }
}

static TInt IdleCompleteReserveCallback(TAny *aData) {
    CCameraPlugin *impl = (CCameraPlugin*)aData;
    impl->HandleReserveComplete(KErrNone);
    
    return 0;
}

static TInt IdleCompletePowerOnCallback(TAny *aData) {
    CCameraPlugin *impl = (CCameraPlugin*)aData;
    impl->HandlePowerOnComplete(KErrNone);
    
    return 0;
}

void CCameraPlugin::Reserve() {
    TInt result = ECamClaim(0, iDispatchInstance);

    if (result != KErrNone) {
        HandleReserveComplete(result);
    } else {
        iEventCompleteIdle->Start(TCallBack(IdleCompleteReserveCallback, this));
    }
}

void CCameraPlugin::Release() {
    ECamRelease(0, iDispatchInstance);
}

void CCameraPlugin::PowerOn() {
    if (iEventCompleteIdle->IsActive()) {
        HandlePowerOnComplete(KErrNotReady);
        return;
    }

    TInt result = ECamPowerOn(0, iDispatchInstance);
    if (result != KErrNone) {
        HandlePowerOnComplete(result);
    } else {
        iEventCompleteIdle->Start(TCallBack(IdleCompletePowerOnCallback, this));
    }
}

void CCameraPlugin::PowerOff() {
    ECamPowerOff(0, iDispatchInstance);
}

TInt CCameraPlugin::Handle() {
    return (TInt)iDispatchInstance;
}

#define CAMIMPL_SET_PARAMETER(key, value, cache, errorMsg)                                  \
    TInt result = ECamSetParameter(0, iDispatchInstance, key, (TInt)value);                       \
    if (result != KErrNone) {                                                               \
        LogOut(KCameraCat, _L(errorMsg));                                                   \
        User::Leave(result);                                                                \
    } else {                                                                                \
        cache = value;                                                                      \
    }

void CCameraPlugin::SetZoomFactorL(TInt aZoomFactor) {
    CAMIMPL_SET_PARAMETER(EOpticalZoom, aZoomFactor, iOpticalZoomFactor, "Unable to set optical zoom factor!");
}

TInt CCameraPlugin::ZoomFactor() const {
    return iOpticalZoomFactor;
}

void CCameraPlugin::SetDigitalZoomFactorL(TInt aDigitalZoomFactor) {
    CAMIMPL_SET_PARAMETER(EDigitalZoom, aDigitalZoomFactor, iDigitalZoomFactor, "Unable to set digital zoom factor!");
}

TInt CCameraPlugin::DigitalZoomFactor() const {
    return iDigitalZoomFactor;
}

void CCameraPlugin::SetContrastL(TInt aContrast) {
    CAMIMPL_SET_PARAMETER(EContrast, aContrast, iContrast, "Unable to set contrast!");
}

TInt CCameraPlugin::Contrast() const {
    return iContrast;
}

void CCameraPlugin::SetBrightnessL(TInt aBrightness) {
    CAMIMPL_SET_PARAMETER(EBrightness, aBrightness, iBrightness, "Unable to set brightness!");
}

TInt CCameraPlugin::Brightness() const {
    return iBrightness;
}

void CCameraPlugin::SetFlashL(TFlash aFlash) {
    CAMIMPL_SET_PARAMETER(EFlash, aFlash, iFlash, "Unable to set flash!");
}

CCamera::TFlash CCameraPlugin::Flash() const {
    return iFlash;
}

void CCameraPlugin::SetExposureL(TExposure aExposure) {
    CAMIMPL_SET_PARAMETER(EExposure, aExposure, iExposure, "Unable to set exposure!");
}

CCamera::TExposure CCameraPlugin::Exposure() const {
    return iExposure;
}

void CCameraPlugin::SetWhiteBalanceL(TWhiteBalance aWhiteBalance) {
    CAMIMPL_SET_PARAMETER(EWhiteBalance, aWhiteBalance, iWhiteBalance, "Unable to set white balance!");
}

CCamera::TWhiteBalance CCameraPlugin::WhiteBalance() const {
    return iWhiteBalance;
}

TAny* CCameraPlugin::CustomInterface(TUid aInterface) {
    return NULL;
}

EXPORT_C TECAMEvent::TECAMEvent(TUid aEventType, TInt aErrorCode)
    : iEventType(aEventType)
    , iErrorCode(aErrorCode) {
}

EXPORT_C TECAMEvent::TECAMEvent() {
}

EXPORT_C TECAMEvent2::TECAMEvent2(TUid aEventType, TInt aErrorCode, TInt aParam)
    : TECAMEvent(aEventType, aErrorCode)
    , iEventClassUsed(KUidECamEventClass2)
    , iParam(aParam) {	
}

EXPORT_C const TUid& TECAMEvent2::EventClassUsed() const {
    return iEventClassUsed;
}

EXPORT_C TBool TECAMEvent2::IsEventEncapsulationValid(const TECAMEvent& aEvent) {
    if ((aEvent.iEventType.iUid >= KECamUidEvent2RangeBegin) && (aEvent.iEventType.iUid <= KECamUidEvent2RangeEnd)) {
        TECAMEvent2 eventv2 = static_cast<const TECAMEvent2&>(aEvent);

        if (eventv2.EventClassUsed().iUid == KUidECamEventClass2UidValue) {
            return ETrue;
        } else {
            return EFalse;
        }
    }

    switch (aEvent.iEventType.iUid) {
    case KUidECamEventCameraSettingPreCaptureWarningUidValue:
    case KUidECamEventCIPSetColorSwapEntryUidValue:
    case KUidECamEventCIPRemoveColorSwapEntryUidValue:
    case KUidECamEventCIPSetColorAccentEntryUidValue:
    case KUidECamEventCIPRemoveColorAccentEntryUidValue: {
        TECAMEvent2 eventv2 = static_cast<const TECAMEvent2&>(aEvent);

        if (eventv2.EventClassUsed().iUid == KUidECamEventClass2UidValue) {
            return ETrue;
        }

        break;
    }

    default:
        break;
    }

    return EFalse;
}

EXPORT_C CCamera* CCamera::New2L(MCameraObserver2& aObserver,TInt aCameraIndex,TInt aPriority) {
    CCameraPlugin *camera = new (ELeave) CCameraPlugin(&aObserver);
    CleanupStack::PushL(camera);
    
    camera->ConstructL(aCameraIndex);
    CleanupStack::Pop(camera);
    
    return camera;
}

EXPORT_C CCamera* CCamera::NewL(MCameraObserver2& aObserver,TInt aCameraIndex,TInt aPriority) {
    return New2L(aObserver, aCameraIndex, aPriority);
}

EXPORT_C CCamera* CCamera::NewL(MCameraObserver& aObserver,TInt aCameraIndex) {
    CCameraPlugin *camera = new (ELeave) CCameraPlugin(&aObserver);
    CleanupStack::PushL(camera);
    
    camera->ConstructL(aCameraIndex);
    CleanupStack::Pop(camera);
    
    return camera;
}

EXPORT_C CCamera* CCamera::NewDuplicate2L(MCameraObserver2& aObserver,TInt aCameraHandle) {
    CCameraPlugin *camera = new (ELeave) CCameraPlugin(&aObserver);
    CleanupStack::PushL(camera);
    
    camera->ConstructSharedL((TAny*)aCameraHandle);
    CleanupStack::Pop(camera);
    
    return camera;
}

EXPORT_C CCamera* CCamera::NewDuplicateL(MCameraObserver2& aObserver,TInt aCameraHandle) {
    return NewDuplicate2L(aObserver, aCameraHandle);
}

EXPORT_C CCamera* CCamera::NewDuplicateL(MCameraObserver& aObserver,TInt aCameraHandle) {
    CCameraPlugin *camera = new (ELeave) CCameraPlugin(&aObserver);
    CleanupStack::PushL(camera);
    
    camera->ConstructSharedL((TAny*)aCameraHandle);
    CleanupStack::Pop(camera);
    
    return camera;
}
