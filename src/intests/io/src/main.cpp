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

void MainWrapperL()
    {
        instance = new (ELeave) CTestManager;
        CleanupStack::PushL(instance);
        
        // Add all tests back
        AddFileTestCasesL();
        
        TInt totalPass = instance->Run();        
        RDebug::Printf("%d tests passed", totalPass);
        
        CleanupStack::PopAndDestroy();
    }

TInt E32Main()
    {
        // Setup the stack
        CCleanup *cleanup = CCleanup::New();
        
        TRAPD(err, MainWrapperL());
        
        // Returns to the dust
        delete cleanup;
        
        return err;
    }
