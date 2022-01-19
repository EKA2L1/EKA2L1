#ifndef GOOMMONITOR_H
#define GOOMMONITOR_H

#include <e32std.h>

class RGOomMonitorSession : public RSessionBase {
public:
    enum TGOomPriority {
        EGOomPriorityNormal = 0,
        EGOomPriorityHigh,
        EGOomPriorityBusy
    };

    EXPORT_C TInt Connect();
    EXPORT_C TInt Connect(TRequestStatus& aStatus);
    EXPORT_C TBool IsConnected();
    EXPORT_C TInt RequestOptionalRam(TInt aBytesRequested, TInt aMinimumBytesNeeded, TInt aPluginId, TInt& aBytesAvailable);
    EXPORT_C void RequestOptionalRam(TInt aBytesRequested, TInt aMinimumBytesNeeded, TInt aPluginId, TRequestStatus& aStatus);
    EXPORT_C TInt RequestFreeMemory(TInt aBytesRequested);
    EXPORT_C void RequestFreeMemory(TInt aBytesRequested, TRequestStatus& aStatus);
    EXPORT_C void CancelRequestFreeMemory();
    EXPORT_C void SetGOomPriority(TGOomPriority aPriority);
    EXPORT_C void ApplicationAboutToStart(const TUid& aAppUid);
    EXPORT_C void UsesAbsoluteMemTargets(TBool aUseAbsoluteAmount = ETrue);
    EXPORT_C void ThisAppIsNotExiting(TInt aWgId);
    EXPORT_C void AppAboutToStart(TRequestStatus& aStatus, const TUid& aAppUid);
    EXPORT_C void MemoryAllocationsComplete();
};

#endif
