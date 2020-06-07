/*
 * bitmap.cpp
 *
 *  Created on: June 25, 2019
 *      Author: fewds
 */

#include <intests/absorber.h>
#include <intests/fbs/bitmap.h>
#include <intests/testmanager.h>

#include <fbs.h>

void FbsBitmapZeroSizeBitmap() {
    // Is it really valid to do this? We gonna find out through this test
    CFbsBitmap *test = new (ELeave) CFbsBitmap;
    TInt err = test->Create(TSize(0, 0), EColor16M);

    const TSize sizeInBitmap = test->SizeInPixels();
    const TInt stride = test->DataStride();

    TBuf8<350> text;
    text.Format(_L8("Try to create a bitmap with zero size and 16 bit color mode, return code %d"), err);

    EXPECT_INPUT_EQUAL_L(text);

    text.Format(_L8("The bitmap zero size after create has stride = %d, height = %d, width = %d"), stride, sizeInBitmap.iHeight,
        sizeInBitmap.iWidth);

    EXPECT_INPUT_EQUAL_L(text);

    delete test;
}

void FbsBitmapCheckSettingsL() {
    TFileName holderFilename;
    TFullName currentProcessName = RProcess().FullName();

    RFs fs;
    fs.Connect(-1);
    
    fs.SetSessionToPrivate(instance->GetWorkingDrive());
    fs.SessionPath(holderFilename);
    holderFilename.Append(_L("assets\\holder.mbm"));

    CFbsBitmap *test = new (ELeave) CFbsBitmap;
    CleanupStack::PushL(test);

    User::LeaveIfError(test->Load(holderFilename));

    TBuf8<250> text;

    // Check stride
    text.Format(_L8("Bitmap stride: %d"), test->DataStride());
    EXPECT_INPUT_EQUAL_L(text);

    // Check display mode
    text.Format(_L8("Display mode: %d"), (int)test->DisplayMode());
    EXPECT_INPUT_EQUAL_L(text);

    // Check size
    text.Format(_L8("Size x %d, size y %d"), test->SizeInPixels().iWidth, test->SizeInPixels().iHeight);
    EXPECT_INPUT_EQUAL_L(text);

    // Try to get a scanline
    RFile expectedBin;

    TInt scanLineStride = test->DataStride();
    TUint8 *scanLineBuf = (TUint8 *)User::Alloc(scanLineStride);
    TPtr8 scanLineBufPtr(scanLineBuf, scanLineStride);

    RDebug::Printf("Before %d", scanLineBufPtr.Size());
    test->GetScanLine(scanLineBufPtr, TPoint(0, 256), scanLineStride, test->DisplayMode());
    RDebug::Printf("After %d", scanLineBufPtr.Size());

    User::LeaveIfError(expectedBin.Replace(fs, _L("E:\\expected\\fbsbitmap\\datainside2.bin"), EFileShareAny | EFileWrite));
    expectedBin.Write(0, scanLineBufPtr);
    expectedBin.Close();

    fs.Close();
    CleanupStack::Pop(test);
}

void AddFbsBitmapTestCasesL() {
    ADD_TEST_CASE_L(BitmapZeroSize, FbsBitmap, FbsBitmapZeroSizeBitmap);
    ADD_TEST_CASE_L(BitmapSettingsValidate, FbsBitmap, FbsBitmapCheckSettingsL);
}
