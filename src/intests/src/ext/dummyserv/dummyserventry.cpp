#include <intests/ext/dummyserv/dummyserv.h>

#include <e32base.h>
#include <e32std.h>

void MainL() {
    CActiveScheduler *scheduler = new (ELeave) CActiveScheduler;
    CleanupStack::PushL(scheduler);

    CActiveScheduler::Install(scheduler);
    CDummyServer::NewLC();

    RProcess::Rendezvous(KErrNone);
    CActiveScheduler::Start();

    CleanupStack::PopAndDestroy(2, scheduler);
}

TInt E32Main() {
    CTrapCleanup *cleanup = CTrapCleanup::New();
    TRAPD(err, MainL());

    delete cleanup;
    return err;
}
