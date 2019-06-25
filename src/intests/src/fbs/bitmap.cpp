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
    
    TBuf8<250> text;
    text.Format(_L8("Try to create a bitmap with zero size and 16 bit color mode, return code %d"), err);
    
    EXPECT_INPUT_EQUAL_L(text);
    
    text.Format(_L8("The bitmap zero size after create has stride = %d, height = %d, width = %d"), stride, sizeInBitmap.iHeight,
        sizeInBitmap.iWidth);
    
    EXPECT_INPUT_EQUAL_L(text);
    
    delete test;
}

void AddFbsBitmapTestCasesL() {
    ADD_TEST_CASE_L(BitmapZeroSize, FbsBitmap, FbsBitmapZeroSizeBitmap);
}
