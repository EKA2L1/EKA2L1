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

#include "BacklightImpl.hpp"
#include <Log.h>

CBackLightControlImpl::CBackLightControlImpl(MBackLightControlObserver* aCallback)
    : iObserver(aCallback)
    , iCaptured(EFalse) {
}

CBackLightControlImpl::~CBackLightControlImpl() {
    CCoeEnv::Static()->RemoveForegroundObserver(*this);
    Close();   
}

void CBackLightControlImpl::ConstructL() {
    Open();
    CCoeEnv::Static()->AddForegroundObserverL(*this);
}

static TInt FormHWRMTargets(TInt aType) {
    switch (aType) {
    case CBackLightControl::EBackLightTypeScreen:
        return EHWRMLightSimpleTargetDisplay;
    
    case CBackLightControl::EBackLightTypeKeys:
        return EHWRMLightSimpleTargetKeyboard;

    case CBackLightControl::EBackLightTypeBoth:
        return EHWRMLightSimpleTargetDisplay | EHWRMLightSimpleTargetKeyboard;

    default:
        break;
    }

    return EHWRMLightSimpleTargetDisplay | EHWRMLightSimpleTargetKeyboard;
}

static void CallBacklightCallbacks(MBackLightControlObserver *aCallback, TInt aType, TInt aStatus) {
    if (!aCallback) {
        return;
    }

    if ((aType == CBackLightControl::EBackLightTypeScreen) || (aType == CBackLightControl::EBackLightTypeBoth)) {
        aCallback->ScreenNotify(aStatus);
    }

    if ((aType == CBackLightControl::EBackLightTypeKeys) || (aType == CBackLightControl::EBackLightTypeBoth)) {
        aCallback->KeysNotify(aStatus);
    }
}

TInt CBackLightControlImpl::BackLightOn(TInt aType,TUint16 aDuration) {
    THWRMLightsOnData onData;
    onData.iTarget = FormHWRMTargets(aType);
    onData.iIntensity = 100;
    onData.iDuration = aDuration;
    onData.iFadeIn = ETrue;
    onData.iColor = 0;      // Default color;

    TInt result = iController.LightOn(onData);
    if (result == KErrNone) {
        CallBacklightCallbacks(iObserver, aType, EBackLightStateOn);
    }

    return result;
}

TInt CBackLightControlImpl::BackLightBlink(TInt aType,TUint16 aDuration,TUint16 aOnTime,TUint16 aOffTime) {
    THWRMLightsBlinkData blinkData;
    blinkData.iTarget = FormHWRMTargets(aType);
    blinkData.iIntensity = 100;
    blinkData.iDuration = aDuration;
    blinkData.iOnCycleDuration = aOnTime;
    blinkData.iOffCycleDuration = aOffTime;
    blinkData.iColor = 0;      // Default color;

    TInt result = iController.LightBlink(blinkData);
    if (result == KErrNone) {
        CallBacklightCallbacks(iObserver, aType, EBackLightStateBlink);
    }

    return result;
}

TInt CBackLightControlImpl::BackLightOff(TInt aType,TUint16 aDuration) {
    THWRMLightsOffData offData;
    offData.iTarget = FormHWRMTargets(aType);
    offData.iDuration = aDuration;
    offData.iFadeOut = ETrue;

    TInt result = iController.LightOff(offData);
    if (result == KErrNone) {
        CallBacklightCallbacks(iObserver, aType, EBackLightStateOff);
    }

    return result;
}

TInt CBackLightControlImpl::BackLightChange(TInt aType,TUint16 aDuration) {
    LogOut(KBacklightCat, _L("Backlight change stubbed"));
    return KErrNone;
}

TInt CBackLightControlImpl::BackLightState(TInt aType) {
    LogOut(KBacklightCat, _L("Backlight state stubbed"));
    return EBackLightStateOn;
}

TInt CBackLightControlImpl::SetScreenBrightness(TInt aState,TUint16 aDuration) {
    LogOut(KBacklightCat, _L("Set screen brightness stubbed"));
    return KErrNone;
}

TInt CBackLightControlImpl::ScreenBrightness() {
    LogOut(KBacklightCat, _L("Screen brightness stubbed"));
    return KErrNone;
}

void CBackLightControlImpl::HandleGainingForeground() {
    Open();
}

void CBackLightControlImpl::HandleLosingForeground() {
    Close();
}

void CBackLightControlImpl::Close() {
    if (iCaptured) {
        iController.Close();
        iCaptured = EFalse;
    }
}

void CBackLightControlImpl::Open() {
    if (!iCaptured) {
        TInt err = iController.Connect();

        if (err == KErrNone) {
            iCaptured = ETrue;
        }
    }
}

CBackLightControl* CBackLightControl::NewL() {
    CBackLightControl *self = NewLC(NULL);
    CleanupStack::Pop();
    return self;
}

CBackLightControl* CBackLightControl::NewL(MBackLightControlObserver* aCallback) {
    CBackLightControl *self = NewLC(aCallback);
    CleanupStack::Pop();
    return self;
}

CBackLightControl* CBackLightControl::NewLC(MBackLightControlObserver* aCallback) {
    CBackLightControlImpl *self = new (ELeave) CBackLightControlImpl(aCallback);
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

CBackLightControl::CBackLightControl()
    : CBase() {
}

CBackLightControl::~CBackLightControl() {
}

GLDEF_C TInt E32Dll(TDllReason /*aReason*/) {
    return KErrNone;
}