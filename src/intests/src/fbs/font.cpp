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

static CFont *FbsGetNearestFont(CFbsTypefaceStore *store, TPtrC name, TInt *err) {
    TFontSpec searchSpec(name, 10);
    CFont *font = NULL;
    *err = store->GetNearestFontToDesignHeightInPixels(font, searchSpec);
    return font;
}

#define BEGIN_FONT_TEST(name)                                    \
    CFbsTypefaceStore *store = CFbsTypefaceStore::NewL(NULL);    \
    CleanupStack::PushL(store);                                  \
    CFont *font = NULL;                                          \
    TInt err = 0;                                                \
    font = FbsGetNearestFont(store, name, &err);                 \
    TBuf8<512> text;                                             \
    if (err != KErrNone) {                                       \
        text.Format(_L8("Can't find sans font, error %d"), err); \
    } else {
#define END_FONT_TEST           \
    }                           \
    EXPECT_INPUT_EQUAL_L(text); \
    store->ReleaseFont(font);   \
    CleanupStack::Pop(store);

void FbsFontstoreGetCharacterDataL() {
    BEGIN_FONT_TEST(_L("Series 60 Sans"))
    TOpenFontCharMetrics metrics;
    const TUint8 *bitmap;
    TSize size;

    CFont::TCharacterDataAvailability avail = font->GetCharacterData('A', metrics, bitmap, size);
    text.Format(_L8("Character A status: %d"), static_cast<int>(avail));

    END_FONT_TEST
}

void FbsFontstoreNearestToDesignWithNameL() {
    BEGIN_FONT_TEST(_L("Series 60 Sans"))

    TOpenFontFaceAttrib attrib;
    ((CFbsFont *)font)->GetFaceAttrib(attrib);

    TOpenFontMetrics metrics;
    ((CFbsFont *)font)->GetFontMetrics(metrics);

    TBuf8<128> fontFullName8;
    CnvUtfConverter::ConvertFromUnicodeToUtf8(fontFullName8, attrib.FullName().Left(22));

    TBuf8<128> fontFamName8;
    CnvUtfConverter::ConvertFromUnicodeToUtf8(fontFamName8, attrib.FamilyName());

    text.Format(_L8("Nearest font to Series 60 Sans is: %S (family: %S). "
                    "Font ascent: %d, descent: %d, max depth: %d, max height: %d,"
                    " max width: %d"),
        &fontFullName8, &fontFamName8, metrics.Ascent(), metrics.Descent(),
        metrics.MaxDepth(), metrics.MaxHeight(), metrics.MaxWidth());

    END_FONT_TEST
}

void AddFbsFontTestCasesL() {
    ADD_TEST_CASE_L(FontStoreNearestToDesignWithName, FbsFontStore, FbsFontstoreNearestToDesignWithNameL);
    ADD_TEST_CASE_L(FontStoreCharacterData, FbsFontStore, FbsFontstoreGetCharacterDataL);
}
