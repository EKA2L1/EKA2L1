/*
 * ws.cpp
 *
 *  Created on: Dec 26, 2018
 *      Author: fewds
 */

#include <intests/testmanager.h>
#include <intests/ws/ws.h>

#include <w32std.h>

void WsGetAllScreenModeL() {
    RWsSession session;
    User::LeaveIfError(session.Connect());

    CWsScreenDevice *device = new (ELeave) CWsScreenDevice(session);
    CleanupStack::PushL(device);
    device->Construct();

    RArray<TInt> modeList;
    device->GetScreenSizeModeList(&modeList);

    TBuf8<270> expectedLine;
    expectedLine.Format(_L8("Total screen mode (wsini.ini): %d"), modeList.Count());

    EXPECT_INPUT_EQUAL_L(expectedLine);

    for (TInt i = 0; i < modeList.Count(); i++) {
        TPixelsAndRotation modeInfo;
        device->GetScreenModeSizeAndRotation(modeList[i], modeInfo);

        TInt rotation = 0;

        switch (modeInfo.iRotation) {
        case CFbsBitGc::EGraphicsOrientationRotated90: {
            rotation = 90;
            break;
        }

        case CFbsBitGc::EGraphicsOrientationRotated180: {
            rotation = 180;
            break;
        }

        case CFbsBitGc::EGraphicsOrientationRotated270: {
            rotation = 270;
            break;
        }

        default:
            break;
        }

        expectedLine.Format(_L8("Mode %d: size = (%d, %d), rot = %d"), i,
            modeInfo.iPixelSize.iHeight, modeInfo.iPixelSize.iWidth, rotation);

        EXPECT_INPUT_EQUAL_L(expectedLine);
    }

    CleanupStack::PopAndDestroy(device);
    session.Close();
}

void AddWsTestCasesL() {
    ADD_TEST_CASE_L(GetAllScreenModeSizeAndRotation, WindowServer, WsGetAllScreenModeL);
}
