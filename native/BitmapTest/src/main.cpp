#include <e32base.h>
#include <e32std.h>
#include <fbs.h>
#include <w32std.h>

#include <eikon.hrh>

void MainL() {
    RFs fs;
    User::LeaveIfError(fs.Connect(-1));

    fs.SetSessionToPrivate(EDriveC);

    RWsSession winSession;
    User::LeaveIfError(winSession.Connect(fs));

    CWsScreenDevice *device = new (ELeave) CWsScreenDevice(winSession);
    CleanupStack::PushL(device);
    device->Construct();

    RWindowGroup *group = new (ELeave) RWindowGroup(winSession);
    CleanupStack::PushL(group);

    group->Construct(0);
    group->CaptureKey(EKeyOK, 0, 0);

    RWindow *win = new (ELeave) RWindow(winSession);
    win->Construct(*group, 2);

    CleanupStack::PushL(win);

    // We will ....
    // First. Not compress
    CFbsBitmap *bitmap = new (ELeave) CFbsBitmap();
    CleanupStack::PushL(bitmap);

    TFileName fileSmile;
    fs.SessionPath(fileSmile);
    fileSmile.Append(_L("assets\\SmileyFace.mbm"));

    bitmap->Resize(TSize(240, 320));

    User::LeaveIfError(bitmap->Load(fileSmile, 0));

    win->SetExtent(TPoint(0, 0), TSize(360, 640));
    win->SetRequiredDisplayMode(EColor64K);

    win->Activate();
    win->SetVisible(true);
    group->SetOrdinalPosition(100, 1000);

    CWindowGc gc(device);
    TInt err = gc.Construct();

    win->Invalidate();
    win->BeginRedraw();

    gc.Activate(*win);

    gc.DrawBitmap(TPoint(0, 0), bitmap);
    gc.Deactivate();

    win->EndRedraw();

    TRequestStatus status;

    winSession.EventReady(&status);
    User::WaitForRequest(status);

    TWsEvent evt;
    winSession.GetEvent(evt);

    win->Destroy();
    group->Destroy();
    winSession.Close();

    bitmap->Reset();

    fs.Close();

    CleanupStack::PopAndDestroy(4);
}

TInt E32Main() {
    CTrapCleanup *cleanup = CTrapCleanup::New();
    TRAPD(err, MainL());

    return err;
}
