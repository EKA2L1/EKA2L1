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

#define IMPORT_C __declspec(dllexport)

#include <cdsb.h>
#include <scdv/log.h>
#include <scdv/panic.h>
#include <scdv/sv.h>

#include <hal.h>

struct CDirectScreenBitmapImpl: public CDirectScreenBitmap {
    TUint32 *iData;
    TSize iSize;
    TUint32 iFlags;
    TInt iScreenNum;

public:
    explicit CDirectScreenBitmapImpl();

    virtual TInt Create(const TRect& aScreenRect, TSettingsFlags aSettingsFlags);
    virtual TInt BeginUpdate(TAcceleratedBitmapInfo& aBitmapInfo);
    virtual void EndUpdate(TRequestStatus& aComplete);
    virtual void EndUpdate(const TRect& aScreenRect, TRequestStatus& aComplete);
    virtual void Close();
};

CDirectScreenBitmapImpl::CDirectScreenBitmapImpl()
    : iData(NULL)
    , iSize(0, 0)
    , iFlags(0)
    , iScreenNum(0) {
}

TInt CDirectScreenBitmapImpl::Create(const TRect& aScreenRect, TSettingsFlags aSettingsFlags) {
    const TInt screenNum = (aSettingsFlags >> 28) & 7;
    HAL::Get(screenNum, HAL::EDisplayMemoryAddress, (TInt &)(iData));

    if (iData == NULL) {
        return KErrArgument;
    }

    // Try to get the width and the height of the screen
    HAL::Get(screenNum, HAL::EDisplayXPixels, iSize.iWidth);
    HAL::Get(screenNum, HAL::EDisplayYPixels, iSize.iHeight);

    // Getting the real flags, ignoring the screen number
    iFlags = (aSettingsFlags & ~(7 << 28));
    iScreenNum = screenNum;

    Scdv::Log("New direct screen bitmap created for screen %d, double buffer: %d", screenNum, iFlags & CDirectScreenBitmap::EDoubleBuffer);

    return KErrNone;
}

TInt CDirectScreenBitmapImpl::BeginUpdate(TAcceleratedBitmapInfo& aBitmapInfo) {
    aBitmapInfo.iDisplayMode = EColor16MA;
    aBitmapInfo.iAddress = reinterpret_cast<TUint8*>(iData);
    aBitmapInfo.iSize = iSize;
    aBitmapInfo.iLinePitch = iSize.iWidth * 4;
    aBitmapInfo.iPixelShift = 5;    // Should be 2^5 = 32

    return KErrNone;
}

void CDirectScreenBitmapImpl::EndUpdate(TRequestStatus& aComplete) {
    TRect rect;
    rect.iTl = TPoint(0, 0);
    rect.iBr = rect.iTl + iSize;

    EndUpdate(rect, aComplete);
}

void CDirectScreenBitmapImpl::EndUpdate(const TRect& aScreenRect, TRequestStatus& aComplete) {
    UpdateScreen(1, iScreenNum, 1, &aScreenRect);

    TRequestStatus *status = &aComplete;
    User::RequestComplete(status, KErrNone);
}

void CDirectScreenBitmapImpl::Close() {
}

CDirectScreenBitmap *CDirectScreenBitmap::NewL() {
    return CDirectScreenBitmap::NewL(0);
}

CDirectScreenBitmap *CDirectScreenBitmap::NewL(const TInt aScreenNum) {
    return new (ELeave) CDirectScreenBitmapImpl();
}
