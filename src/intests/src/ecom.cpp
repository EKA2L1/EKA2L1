/*
 * ws.cpp
 *
 *  Created on: Feb 18, 2019
 *      Author: fewds
 */

#include <ecom/ecom.h>
#include <intests/testmanager.h>

#include <e32base.h>
#include <e32std.h>

#include <fbs.h>

/**
 * \brief Get all OpenFont rasterizer plugin through ECom
 * 
 * This test case assumes that no other plugins are installed on other drive, which means this
 * only count plugins on ROM.
 */
void EComGetFontRasterizerPluginInfosL() {
    RImplInfoPtrArray infos;
    TUid openFontUid = { KUidOpenFontRasterizerPlunginInterface };

    REComSession::ListImplementationsL(openFontUid, infos);

    TBuf8<256> expectLine;
    expectLine.Format(_L8("Total Openfont Rasterizer plugins %d"), infos.Count());

    EXPECT_INPUT_EQUAL_L(expectLine);

    for (TInt i = 0; i < infos.Count(); i++) {
        const TDesC &name = infos[i]->DisplayName();
        TDriveUnit unit = infos[i]->Drive();
        expectLine.Format(_L8("Openfont Rasterizer Plugin %d: %S, drive %S, uid 0x%08x"), i, &name, &unit.Name(), infos[i]->ImplementationUid().iUid);

        EXPECT_INPUT_EQUAL_L(expectLine);
    }
}

/**
 * \brief Get all OpenFont TrueType plugin through ECom.
 * 
 * This test case assumes that no other plugins are installed on other drive, which means this
 * only count plugins on ROM.
 */
void EComGetFontTrueTypePluginInfosL() {
    RImplInfoPtrArray infos;
    REComSession::ListImplementationsL(KUidOpenFontTrueTypeExtension, infos);

    TBuf8<256> expectLine;
    expectLine.Format(_L8("Total Openfont TrueType plugins %d"), infos.Count());

    EXPECT_INPUT_EQUAL_L(expectLine);

    for (TInt i = 0; i < infos.Count(); i++) {
        const TDesC &name = infos[i]->DisplayName();
        TDriveUnit unit = infos[i]->Drive();
        expectLine.Format(_L8("Openfont TrueType Plugin %d: %S, drive %S, uid 0x%08x"), i, &name, &unit.Name(), infos[i]->ImplementationUid().iUid);

        EXPECT_INPUT_EQUAL_L(expectLine);
    }

    REComSession::ListImplementationsL(KUidOpenFontTrueTypeExtension, infos);
}

TInt ImplUidLessThan(const CImplementationInformation& aLhs, const CImplementationInformation& aRhs) {
    return aLhs.ImplementationUid().iUid - aRhs.ImplementationUid().iUid;
}

void EComGetCdlPluginInfosL() {
    RImplInfoPtrArray infos;
    REComSession::ListImplementationsL(TUid::Uid(0x101f8243), infos);

    infos.Sort(&ImplUidLessThan);
    
    TBuf8<256> expectLine;
    expectLine.Format(_L8("Total CDL plugins %d"), infos.Count());

    EXPECT_INPUT_EQUAL_L(expectLine);

    for (TInt i = 0; i < infos.Count(); i++) {
        const TDesC &name = infos[i]->DisplayName();
        TDriveUnit unit = infos[i]->Drive();
        expectLine.Format(_L8("CDL Plugin %d: %S, drive %S, uid 0x%08x"), i, &name, &unit.Name(), infos[i]->ImplementationUid().iUid);

        EXPECT_INPUT_EQUAL_L(expectLine);
    }
}

void AddEComTestCasesL() {
    ADD_TEST_CASE_L(GetAllFontRasterizerPluginInstall, EComServer, EComGetFontRasterizerPluginInfosL);
    ADD_TEST_CASE_L(GetAllFontTrueTypePluginInstall, EComServer, EComGetFontTrueTypePluginInfosL);
    ADD_TEST_CASE_L(GetCdlPluginInstall, EComServer, EComGetCdlPluginInfosL);
}
