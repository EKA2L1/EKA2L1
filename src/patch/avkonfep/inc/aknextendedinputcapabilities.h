#ifndef AKNEXTENDEDINPUTCAPABILITIES_H
#define AKNEXTENDEDINPUTCAPABILITIES_H

#include <e32base.h>
#include <coemop.h>
#include <w32std.h>

NONSHARABLE_CLASS(CAknExtendedInputCapabilities) : public CBase {
public:
    DECLARE_TYPE_ID(0x10282348)

    class MAknEventObserver {
    public:
        enum TInputCapabilitiesEvent {
            EActivatePenInputRequest,
            EPointerEventReceived,
            EControlContentUpdatedInternally,
            EOpenStylusMenuCcpu
        };

        virtual void HandleInputCapabilitiesEventL(TInt aEvent, TAny* aParams) = 0;
    };

    IMPORT_C void RegisterObserver(MAknEventObserver* aObserver);
    IMPORT_C void UnregisterObserver(MAknEventObserver* aObserver);
};

#endif // AKNEXTENDEDINPUTCAPABILITIES_H