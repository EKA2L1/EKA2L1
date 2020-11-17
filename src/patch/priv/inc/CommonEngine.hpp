/*
    CommonEngine.hpp
    Copyright (C) 2005 zg
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#ifndef __COMMONENGINE_HPP__
#define __COMMONENGINE_HPP__

#include <e32base.h>
#include <bamdesca.h>

class MSharedDataNotifyHandler
{
  public:
    virtual void HandleNotifyL(TUid anUid,const TDesC16& aKey,const TDesC16& aValue)=0;
};

class RSharedDataClient: public RSessionBase
{
  public:
    RSharedDataClient(void);
    RSharedDataClient(MSharedDataNotifyHandler* aHandler);

    TInt AssignToTemporaryFile(TUid anUid) const;
    TInt Assign(TUid anUid) const;
    void CancelAllNotifies(void);
    void CancelFreeDiskSpaceRequest(void) const;
    void CancelNotify(TUid anUid,TDesC16 const* aValue);
    void Close(void);
    TInt Connect(TInt aParam);
    TInt Flush(void) const;
    TInt GetInt(TDesC16 const& aName, TInt& aValue) const;
    TInt GetReal(TDesC16 const& aName, TReal& aValue) const;
    TInt GetString(TDesC16 const& aName, TDes16& aValue) const;
    TInt NotifyChange(TUid anUid, TDesC16 const* aValue);
    TInt NotifySet(TUid anUid, TDesC16 const* aValue);
    void RequestFreeDiskSpaceLC(TInt aSize) const;
    void RequestFreeDiskSpace(TInt aSize) const;
    TInt RestoreOriginal(TUid anUid,MDesC16Array const* anArray);
    TInt SetInt(TDesC16 const& aName,TInt aValue);
    TInt SetReal(TDesC16 const& aName,TReal aValue);
    TInt SetString(TDesC16 const& aName,TDesC16 const& aValue);
    TInt AddToValue(TDesC16 const& aName,TInt aValue);
    void CancelAllSignals(void);
    void CancelSignal(TDesC16 const& aName);
    TInt Signal(TDesC16 const& aName);
  private:
    CActive* iOpened;
    MSharedDataNotifyHandler* iHandler;
    TInt iConnectParam;
  private:
    void PanicIfRMinus(TDesC16 const& aName);
    TVersion Version(void) const;
    TInt RunServer(void);
};

#endif