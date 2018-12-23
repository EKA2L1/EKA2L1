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
#include <intests/io/file.h>
#include <intests/kern/codeseg.h>
#include <intests/ipc/ipc.h>

#include <intests/testmanager.h>

#define GEN_TESTS 0

void MainWrapperL()
    {
#if GEN_TESTS
		TAbsorberMode mode = EAbsorbWrite;
#else
		TAbsorberMode mode = EAbsorbVerify;
#endif
				
        instance = CTestManager::NewLC(mode);
        
        // Add all tests back
        AddIpcTestCasesL();
        AddCodeSegTestCasesL();
        AddCmdTestCaseL();
        AddFileTestCasesL();
        
        TInt totalPass = instance->Run();    
        RDebug::Printf("%d tests passed", totalPass);
        
        CleanupStack::PopAndDestroy();
    }

TInt E32Main()
    {
        // Setup the stack
        CTrapCleanup *cleanup = CTrapCleanup::New();
        TRAPD(err, MainWrapperL());
        
        // Returns to the dust
        delete cleanup;
        
        return err;
    }
