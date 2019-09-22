#include <bitstd.h>
#include <bitdev.h>
#include <e32std.h>
#include <e32base.h>
#include <fbs.h>

void DrawStuff() {
    RFs fs;
    User::LeaveIfError(fs.Connect(-1));

    fs.SetSessionToPrivate(EDriveC);
    
    RFbsSession::Connect(fs);
    
    CFbsFont* font;
    CFbsTypefaceStore* ts = CFbsTypefaceStore::NewL(NULL);
    
    CleanupStack::PushL(ts);
    
    //Font size determined visually
    TFontSpec spec(_L("Series 60 Sans"), 40);
    
    User::LeaveIfError(ts->GetNearestFontToDesignHeightInPixels((CFont*&)font, spec));
    
    TOpenFontCharMetrics metrics;
    const TUint8 *bmpData;
    TSize dataSize;
    font->GetCharacterData('H', metrics, bmpData, dataSize);
    
    RDebug::Print(_L("Getting H character, horiz adv %d, bearing X %d, bearing Y %d, vert adv %d, width %d height %d"),
            metrics.HorizAdvance(), metrics.HorizBearingX(), metrics.HorizBearingY(), metrics.VertAdvance(), metrics.Width(),
            metrics.Height());
    
    RDebug::Print(_L("Baseline off %d, data size x %d, data size y %d"), font->BaselineOffsetInPixels(), dataSize.iWidth, dataSize.iHeight);

    TFileName holderFilename;
    fs.SessionPath(holderFilename);
    holderFilename.Append(_L("assets\\holder.mbm"));
    
    CFbsBitmap *holderBitmap = new (ELeave) CFbsBitmap;
    CleanupStack::PushL(holderBitmap);
    User::LeaveIfError(holderBitmap->Load(holderFilename));
    
    holderBitmap->Save(_L("E:\\Beautiful2.mbm"));
    
    CFbsBitmap *bitmap = new (ELeave) CFbsBitmap;
    CleanupStack::PushL(bitmap);
    
    User::LeaveIfError(bitmap->Create(TSize(700, 700), EColor16M));
    
    CFbsBitmapDevice *bitmapDevice = CFbsBitmapDevice::NewL(bitmap);
    CleanupStack::PushL(bitmapDevice);
    
    CFbsBitGc *bitmapGc = CFbsBitGc::NewL();
    CleanupStack::PushL(bitmapGc);
    
    bitmapGc->Activate(bitmapDevice);
    bitmapGc->UseFont((CFont*)font);
    bitmapGc->DrawText(_L("HIIIII!"), TPoint(112, 105));
    bitmapGc->DiscardFont();
    bitmapGc->BitBlt(TPoint(200, 200), holderBitmap);
    
    bitmap->Save(_L("E:\\Beautiful.mbm"));
    
    CleanupStack::Pop(5);
}

TInt E32Main() {
    CTrapCleanup *cleanup = CTrapCleanup::New();
    TRAPD(err, DrawStuff());
    
    delete cleanup;
    return err;
}
