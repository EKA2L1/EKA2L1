/*
 * main.cpp
 *
 *  Created on: Dec 19, 2018
 *      Author: fewds
 */

#include <e32base.h>
#include <e32debug.h>
#include <e32std.h>

#include <file.h>
#include <testmanager.h>

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
