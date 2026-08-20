#ifndef EKA2L1_PATCH_CDSB_H
#define EKA2L1_PATCH_CDSB_H

#include <e32base.h>
#include <bitdev.h>

// CDirectScreenBitmap is a partner API whose header is not shipped in the
// public Belle SDK. Keep the ABI declaration local so the replacement screen
// driver can provide the ROM exports without depending on partner headers.
class CDirectScreenBitmap : public CBase {
public:
    enum TSettingsFlags {
        ENone = 0,
        EDoubleBuffer = 1,
        EIncrementalUpdate = 2
    };

    static CDirectScreenBitmap *NewL();
    static CDirectScreenBitmap *NewL(TInt aScreenNo);

    virtual TInt Create(const TRect &aScreenRect, TSettingsFlags aSettingsFlags) = 0;
    virtual TInt BeginUpdate(TAcceleratedBitmapInfo &aBitmapInfo) = 0;
    virtual void EndUpdate(TRequestStatus &aComplete) = 0;
    virtual void EndUpdate(const TRect &aScreenRect, TRequestStatus &aComplete) = 0;
    virtual void Close() = 0;
};

#endif
