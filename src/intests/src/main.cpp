/*
 * main.cpp
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#include <e32base.h>
#include <e32debug.h>
#include <e32std.h>

// Add test file here
#include <intests/cmd/cmd.h>
#include <intests/ecom.h>
#include <intests/io/file.h>
#include <intests/ipc/ipc.h>
#include <intests/kern/codeseg.h>
#include <intests/ws/ws.h>

#include <intests/testmanager.h>

#include <w32std.h>

#define GEN_TESTS 0

void MainWrapperL() {
#if GEN_TESTS
    TAbsorberMode mode = EAbsorbWrite;
#else
    TAbsorberMode mode = EAbsorbVerify;
#endif

    instance = CTestManager::NewLC(mode);

    // Add all tests back
    AddWsTestCasesL();
    AddIpcTestCasesL();
    AddCodeSegTestCasesL();
    AddCmdTestCaseL();
    AddFileTestCasesL();
    AddEComTestCasesL();

    TInt totalPass = instance->Run();
    RDebug::Printf("%d/%d tests passed", totalPass, instance->TotalTests());

    CleanupStack::PopAndDestroy();
}

TInt E32Main() {
    // Setup the stack
    CTrapCleanup *cleanup = CTrapCleanup::New();
    TRAPD(err, MainWrapperL());

    // Returns to the dust
    delete cleanup;

    return err;
}
