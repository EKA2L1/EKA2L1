/*
 ============================================================================
 Name		: EKA2L1HW.cpp
 Author	  : 
 Copyright   : Copyright 2018 Do Trong Thu
 Description : Exe source file
 ============================================================================
 */

//  Include Files

#include "EKA2L1HW.h"
#include "EKA2L1Mem.h"

#include <e32base.h>
#include <e32cons.h> // Console
#include <e32std.h>

//  Constants

_LIT(KTextConsoleTitle, "Console");
_LIT(KTextFailed, " failed, leave code = %d");
_LIT(KTextPressAnyKey, " [press any key]\n");

//  Global Variables

LOCAL_D CConsoleBase *console; // write all messages to this

//  Local Functions

LOCAL_C void MainL() {
    // Note that I intend to use a descriptor, though I'm lazy and I want to know where static data relies

    TUint32 consoleAddress = (TUint32)console;
    TPtrC consoleRegion = TranslateMemoryRegion(consoleAddress);

    TAny *testPtr = User::AllocL(25);

    TUint32 heapLocation = (TUint32)testPtr;
    TPtrC heapRegion = TranslateMemoryRegion(heapLocation);

    TUint32 sdataLocation = (TUint32)(consoleRegion.Ptr());
    TPtrC sdataRegion = TranslateMemoryRegion(sdataLocation);

    console->Write(_L("Hello, world!\n"));

    console->Printf(_L("Console location: 0x%08x, Memory Region: %s\n"), consoleAddress, consoleRegion.Ptr());
    console->Printf(_L("Heap alloc location: 0x%08x, Memory Region: %s\n"), heapLocation, heapRegion.Ptr());
    console->Printf(_L("CRS location: 0x%08x, Memory Region: %s\n"), sdataLocation, sdataRegion.Ptr());

    // Free the pointer
    User::Free(testPtr);

    // After the test
    // => User memory are allocated locally
    // => Static data relies on RAM loaded code after the code section
    // => RAM code is pretty far away from local region
}

LOCAL_C void DoStartL() {
    // Create active scheduler (to run active objects)
    CActiveScheduler *scheduler = new (ELeave) CActiveScheduler();
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);

    MainL();

    // Delete active scheduler
    CleanupStack::PopAndDestroy(scheduler);
}

//  Global Functions

GLDEF_C TInt E32Main() {
    // Create cleanup stack
    __UHEAP_MARK;
    CTrapCleanup *cleanup = CTrapCleanup::New();

    // Create output console
    TRAPD(
        createError,
        console = Console::NewL(KTextConsoleTitle,
            TSize(KConsFullScreen, KConsFullScreen)));
    if (createError) {
        delete cleanup;
        return createError;
    }

    // Run application code inside TRAP harness, wait keypress when terminated
    TRAPD(mainError, DoStartL());
    if (mainError)
        console->Printf(KTextFailed, mainError);
    console->Printf(KTextPressAnyKey);
    console->Getch();

    delete console;
    delete cleanup;
    __UHEAP_MARKEND;
    return KErrNone;
}
