/*
    vibractrl.cpp
    Copyright (C) 2005 zg
    Copyright (C) 2020- EKA2L1 Project.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "vibraimpl.hpp"

const TInt KProfileEngUidValue = 0x100058FA;
const TUid KProfileEngUid = {KProfileEngUidValue};

_LIT(KKeyVibrAlert, "VibrAlert");
_LIT(KVibraPanic, "VIBRA-CTRL");

GLDEF_C TInt E32Dll(TDllReason /*aReason*/)
{
  return KErrNone;
}

const TDesC8& CVibraControlImpl::Copyright(void)
{
  _LIT8(KAppCopyright,"vibractrl. (c) 2005-2007 by zg, 2020- by EKA2L1. version 3.20");
  return KAppCopyright;
}

CVibraControl::CVibraControl(): CBase()
{
}

CVibraControl::~CVibraControl()
{
}

void Panic(CVibraControl::TVibraCtrlPanic aPanic)
{
  User::Panic(KVibraPanic,aPanic);
}

EXPORT_C void CVibraControlImpl::StartVibraL(TUint16 aDuration)
{
  User::LeaveIfError(iVibraController.StartVibra(aDuration));
  
  if (iCallback)
    iCallback->VibraRequestStatus(EVibraRequestOK);
}

EXPORT_C void CVibraControlImpl::StopVibraL(void)
{
  User::LeaveIfError(iVibraController.StopVibra());

  if (iCallback)
    iCallback->VibraRequestStatus(EVibraRequestStopped);
}

EXPORT_C CVibraControl::TVibraModeState CVibraControlImpl::VibraSettings(void) const
{
  return iVibraState;
}

EXPORT_C void CVibraControlImpl::StartVibraL(TUint16 aDuration,TInt aIntensity)
{
  User::LeaveIfError(iVibraController.StartVibra(aDuration, aIntensity));
  
  if (iCallback)
    iCallback->VibraRequestStatus(EVibraRequestOK);
}

void CVibraControlImpl::Open() {
  if (!iCaptured) {
    TInt err = iVibraController.Connect();

    if (err == KErrNone) {
        iCaptured = ETrue;
    }
  }
}

void CVibraControlImpl::Close() {
  if (iCaptured) {
      iVibraController.Close();
      iCaptured = EFalse;
  }
}

void CVibraControlImpl::HandleGainingForeground(void)
{
  Open();
}

void CVibraControlImpl::HandleLosingForeground(void)
{
  StopVibraL(); //never leave
  Close();
}

void CVibraControlImpl::HandleNotifyL(TUid anUid,const TDesC16& aKey,const TDesC16& aValue)
{
  if (anUid==KProfileEngUid && aKey==KKeyVibrAlert) {
    if (aValue[0]==0x30)
      iVibraState=EVibraModeOFF;
    else
      iVibraState=EVibraModeON;
    
    if (iCallback) {
      iCallback->VibraModeStatus(iVibraState);
    }
  }
}

CVibraControlImpl::CVibraControlImpl(MVibraControlObserver* aCallback)
  : CVibraControl(),
    iCallback(aCallback),
    iShared(this),
    iVibraState(EVibraModeUnknown) {
}

void CVibraControlImpl::ConstructL(void)
{
  iProfileAvailable = EFalse;
  iCaptured = EFalse;

  TInt err = iShared.Assign(KProfileEngUid);
  TInt vibra = 0;

  if(err==KErrNone)
  {
    err=iShared.GetInt(KKeyVibrAlert,vibra);
    if(err==KErrNone)
    {
      err=iShared.NotifyChange(KProfileEngUid,&KKeyVibrAlert);
      iProfileAvailable = ETrue;
    }
  }

  if(err==KErrNoMemory)
  {
    iShared.Close();
    User::Leave(err);
  }

  iVibraState=vibra?EVibraModeON:EVibraModeOFF;
  Open();

  CCoeEnv::Static()->AddForegroundObserverL(*this);
}

CVibraControlImpl::~CVibraControlImpl()
{
  StopVibraL();
  iShared.Close();

  CCoeEnv::Static()->RemoveForegroundObserver(*this);
  Close();
}

EXPORT_C CVibraControl* VibraFactory::NewL(void)
{
  CVibraControl* self=NewLC(NULL);
  CleanupStack::Pop();
  return self;
}

EXPORT_C CVibraControl* VibraFactory::NewL(MVibraControlObserver* aCallback)
{
  CVibraControl* self=NewLC(aCallback);
  CleanupStack::Pop();
  return self;
}

EXPORT_C CVibraControl* VibraFactory::NewLC(MVibraControlObserver* aCallback)
{
  CVibraControlImpl* self=new(ELeave)CVibraControlImpl(aCallback);
  CleanupStack::PushL(self);
  self->ConstructL();
  return self;
}