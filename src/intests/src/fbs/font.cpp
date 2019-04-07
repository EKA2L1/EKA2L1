/*
 * font.cpp
 *
 *  Created on: Apr 7, 2019
 *      Author: fewds
 */

#include <intests/absorber.h>
#include <intests/fbs/font.h>
#include <intests/testmanager.h>

#include <fbs.h>
#include <openfont.h>
#include <utf.h>

void FbsFontstoreNearestToDesignWithNameL() {
    // Connect!
    RFbsSession::Connect();
    CFbsTypefaceStore *store = CFbsTypefaceStore::NewL(NULL);

    CleanupStack::PushL(store);

    TFontSpec searchSpec(_L("Series 60 Sans"), 1);
    CFont *font = NULL;

    TInt err = store->GetNearestFontToDesignHeightInPixels(font, searchSpec);

    TBuf8<512> text;

    if (err != KErrNone) {
        text.Format(_L8("Can't find sans font, error %d"), err);
    } else {
        TOpenFontFaceAttrib attrib;
        ((CFbsFont *)font)->GetFaceAttrib(attrib);

        TOpenFontMetrics metrics;
        ((CFbsFont *)font)->GetFontMetrics(metrics);

        TBuf8<128> fontFullName8;
        CnvUtfConverter::ConvertFromUnicodeToUtf8(fontFullName8, attrib.FullName().Left(22));

        TBuf8<128> fontFamName8;
        CnvUtfConverter::ConvertFromUnicodeToUtf8(fontFamName8, attrib.FamilyName());

        text.Format(_L8("Nearest font to Series 60 Sans is: %S (family: %S)\n"
                        "Font ascent: %d, descent: %d, max depth: %d, max height: %d,"
                        " max width: %d"),
            &fontFullName8, &fontFamName8, metrics.Ascent(), metrics.Descent(),
            metrics.MaxDepth(), metrics.MaxHeight(), metrics.MaxWidth());
    }

    EXPECT_INPUT_EQUAL_L(text);

    store->ReleaseFont(font);
    CleanupStack::Pop(store);
}

void AddFbsFontTestCasesL() {
    ADD_TEST_CASE_L(FontStoreNearestToDesignWithName, FbsFontStore, FbsFontstoreNearestToDesignWithNameL);
}
