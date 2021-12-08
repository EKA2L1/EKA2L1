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

static const TUint KUidECamCameraIndex0Reserved = 0x102831F1;
static const TUint KUidECamCameraIndex1Reserved = 0x102831FA;
static const TUint KUidECamCameraIndex2Reserved = 0x102831FB;
static const TUint KUidECamCameraIndex3Reserved = 0x102831FC;
static const TUint KUidECamCameraIndex4Reserved = 0x102831FD;
static const TUint KUidECamCameraPluginSecureId = 0xEE000006;

CCameraStatusWatch::CCameraStatusWatch(MReserveObserver& aReserveObserver, TInt aCameraIndex, TInt aSecureId)
    : CActive(EPriorityNormal)
    , iReserveObserver(aReserveObserver)
    , iCameraIndex(aCameraIndex)
    , iSecureId(aSecureId) {
}

EXPORT_C CCameraStatusWatch::~CCameraStatusWatch() {
    Cancel();
    iProperty.Close();
}

void CCameraStatusWatch::ConstructL() {
    switch(iCameraIndex) {
    case 0:
        User::LeaveIfError(iProperty.Attach(TUid::Uid(iSecureId), KUidECamCameraIndex0Reserved));
        break;  

    case 1:
        User::LeaveIfError(iProperty.Attach(TUid::Uid(iSecureId), KUidECamCameraIndex1Reserved));
        break;

    case 2:
        User::LeaveIfError(iProperty.Attach(TUid::Uid(iSecureId), KUidECamCameraIndex2Reserved));
        break;

    case 3:
        User::LeaveIfError(iProperty.Attach(TUid::Uid(iSecureId), KUidECamCameraIndex3Reserved));
        break;

    case 4:
        User::LeaveIfError(iProperty.Attach(TUid::Uid(iSecureId), KUidECamCameraIndex4Reserved));
        break;

    default:
        User::Leave(KErrArgument);
    }

    CActiveScheduler::Add(this);
    
    iProperty.Subscribe(iStatus);
    SetActive();
}

void CCameraStatusWatch::RunL() {
    TBool isReserved = EFalse;
    TInt error = iProperty.Get(isReserved);
    
    TECamReserveStatus reserveStatus;
    if(error != KErrNone) {
        reserveStatus = ECameraStatusUnknown;
    } else {
        if (isReserved) {
            reserveStatus = ECameraReserved;
        } else {
            reserveStatus = ECameraUnReserved;
        }
    }
    
    iReserveObserver.ReserveStatus(iCameraIndex, reserveStatus, error);
    
    if(error == KErrNone) {
        iProperty.Subscribe(iStatus);
        SetActive();
    }
}

void CCameraStatusWatch::DoCancel() {
    iProperty.Cancel();
}

EXPORT_C CCameraStatusWatch* CCameraStatusWatch::NewL(MReserveObserver& aReserveObserver, TInt aCameraIndex, TInt aSecureId) {
    CCameraStatusWatch* watch = new(ELeave) CCameraStatusWatch(aReserveObserver, aCameraIndex, aSecureId);
    CleanupStack::PushL(watch);

    watch->ConstructL();
    CleanupStack::Pop();

    return watch;
}

EXPORT_C void TReservedInfo::SubscribeReserveInfoL(MReserveObserver& aReserveObserver, TInt aCameraIndex, CCameraStatusWatch*& aCameraStatusWatch) {
    aCameraStatusWatch = CCameraStatusWatch::NewL(aReserveObserver, aCameraIndex, KUidECamCameraPluginSecureId);
}
