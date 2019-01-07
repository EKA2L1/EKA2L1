/*
 ============================================================================
 Name		: CmdReversing.cpp
 Author	  : Your friendly mama
 Copyright   : I'm your mama
 Description : Exe source file
 ============================================================================
 */

//  Include Files

#include "CmdReversing.h"
#include <e32base.h>
#include <e32cons.h> // Console
#include <e32std.h>

//  Constants

_LIT(KTextConsoleTitle, "Console");
_LIT(KTextFailed, " failed, leave code = %d");
_LIT(KTextPressAnyKey, " [press any key]\n");
_LIT(KTextHolder, "Lil Tay\n");
_LIT(KTextCommandLineLength, "Command line length = %d \n");
_LIT(KTextAllocError, "Alloc error = %d \n");
_LIT(KTextCmdMsg, "Command line length = %d, msg = %s \n");
_LIT(KTextSlotMsg, "Slot %d: size = %d \n");
_LIT(KTextNotFill, "Slot %d: empty \n");

LOCAL_D CConsoleBase *console;

LOCAL_C void PrintSlotLength(TInt aSlot) {
    TInt slotLength = User::ParameterLength(aSlot);

    if (slotLength == KErrNotFound) {
        console->Printf(KTextNotFill, aSlot);
        return;
    }

    console->Printf(KTextSlotMsg, aSlot, slotLength);
}

LOCAL_C void MainL() {
    console->Write(KTextHolder);

    TInt cmdLength = User::CommandLineLength();

    if (cmdLength == KErrNone) {
        console->Printf(KTextCommandLineLength, cmdLength);
    }

    HBufC *desCommand;
    TRAPD(allocError, desCommand = HBufC::NewL(32));

    if (allocError != KErrNone) {
        console->Printf(KTextAllocError, allocError);
        return;
    }

    _LIT(KTextBufLoc, "Buf location: %d, ptr text: %d \n");

    TPtr mdes(desCommand->Des());

    console->Printf(KTextBufLoc, (TUint64)desCommand, (TUint64)(mdes.Ptr()));
    User::CommandLine(mdes);

    console->Printf(KTextCmdMsg, mdes.Length(), mdes.Ptr());

    for (TUint8 i = 0; i < 16; i++) {
        PrintSlotLength(i);
    }

    HBufC *bufContent;
    TRAP(allocError, bufContent = HBufC::NewL(100));

    TPtr desContent(bufContent->Des());

    TInt slotContent = User::GetDesParameter(1, desContent);
    _LIT(KTextSlot1Content, "Slot 1 string length: %d, string: %s \n");

    if (slotContent == KErrNone) {
        console->Printf(KTextSlot1Content, desContent.Length(), desContent.Ptr());
    }

    delete desCommand;
}

LOCAL_C void DoStartL() {
    CActiveScheduler *scheduler = new (ELeave) CActiveScheduler();
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);

    MainL();

    // Delete active scheduler
    CleanupStack::PopAndDestroy(scheduler);
}

GLDEF_C TInt E32Main() {
    __UHEAP_MARK;
    CTrapCleanup *cleanup = CTrapCleanup::New();

    TRAPD(
        createError,
        console = Console::NewL(KTextConsoleTitle,
            TSize(KConsFullScreen, KConsFullScreen)));
    if (createError) {
        delete cleanup;
        return createError;
    }

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
