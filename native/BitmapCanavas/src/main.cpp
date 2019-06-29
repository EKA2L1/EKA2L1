#include <bitstd.h>
#include <bitdev.h>
#include <e32std.h>
#include <e32base.h>
#include <fbs.h>

void DrawStuff() {
    RFbsSession::Connect();
    
    CFbsFont* font;
    CFbsTypefaceStore* ts = CFbsTypefaceStore::NewL(NULL);
    
    CleanupStack::PushL(ts);
    
    //Font size determined visually
    TFontSpec spec(_L("Series 60 Sans"), 20);
    
    User::LeaveIfError(ts->GetNearestFontToDesignHeightInPixels((CFont*&)font, spec));
    
    CFbsBitmap *bitmap = new (ELeave) CFbsBitmap;
    CleanupStack::PushL(bitmap);
    
    User::LeaveIfError(bitmap->Create(TSize(400, 400), EColor16M));
    
    CFbsBitmapDevice *bitmapDevice = CFbsBitmapDevice::NewL(bitmap);
    CleanupStack::PushL(bitmapDevice);
    
    CFbsBitGc *bitmapGc = CFbsBitGc::NewL();
    CleanupStack::PushL(bitmapGc);
    
    bitmapGc->Activate(bitmapDevice);
    bitmapGc->UseFont((CFont*)font);
    bitmapGc->DrawText(_L("HIIIII!"), TPoint(112, 105));
    bitmapGc->DiscardFont();
    
    bitmap->Save(_L("E:\\Beautiful.mbm"));
    
    CleanupStack::Pop(4);
}

TInt E32Main() {
    CTrapCleanup *cleanup = CTrapCleanup::New();
    TRAPD(err, DrawStuff());
    
    delete cleanup;
    return err;
}
