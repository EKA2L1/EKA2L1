
#include <goommonitor.h>

enum TGOomMonitorOpcode {
    EGOomMonitorRequestFreeMemory,
    EGOomMonitorMemoryAllocationsComplete,
    EGOomMonitorCancelRequestFreeMemory,
    EGOomMonitorThisAppIsNotExiting,
    EGOomMonitorRequestOptionalRam,
    EGOomMonitorSetPriorityBusy,
    EGOomMonitorSetPriorityNormal,
    EGOomMonitorSetPriorityHigh,
    EGoomMonitorAppAboutToStart,
    EGoomMonitorAppUsesAbsoluteMemTargets
};

_LIT(KGraphicsMemoryMonitorServerName, "GOomMonitorServer");

EXPORT_C TInt RGOomMonitorSession::Connect() {
    return CreateSession(KGraphicsMemoryMonitorServerName, TVersion(0,0,0));
}

EXPORT_C TBool RGOomMonitorSession::IsConnected() {
    return ETrue;
}

EXPORT_C TInt RGOomMonitorSession::RequestFreeMemory(TInt aBytesRequested) {
    TIpcArgs p(aBytesRequested,0);
    return SendReceive(EGOomMonitorRequestFreeMemory, p);
}

EXPORT_C void RGOomMonitorSession::MemoryAllocationsComplete(){
    SendReceive(EGOomMonitorMemoryAllocationsComplete);
}

EXPORT_C TInt RGOomMonitorSession::RequestOptionalRam(TInt aBytesRequested, TInt aMinimumBytesNeeded, TInt aPluginId, TInt& aBytesAvailable) {
    TIpcArgs p(aBytesRequested, aMinimumBytesNeeded, aPluginId, aBytesAvailable);
    TInt ret = SendReceive(EGOomMonitorRequestOptionalRam, p);
    if (ret >= 0) {
        aBytesAvailable = ret;
        ret = KErrNone;
    }

    return ret;
}    

EXPORT_C void RGOomMonitorSession::RequestOptionalRam(TInt aBytesRequested, TInt aMinimumBytesNeeded, TInt aPluginId, TRequestStatus& aStatus) {
    TIpcArgs p(aBytesRequested, aMinimumBytesNeeded, aPluginId);
    SendReceive(EGOomMonitorRequestOptionalRam, p, aStatus);
}

EXPORT_C void RGOomMonitorSession::RequestFreeMemory(TInt aBytesRequested, TRequestStatus& aStatus){
    TIpcArgs p(aBytesRequested,0);
    SendReceive(EGOomMonitorRequestFreeMemory, p, aStatus);
}

EXPORT_C void RGOomMonitorSession::CancelRequestFreeMemory() {
    SendReceive(EGOomMonitorCancelRequestFreeMemory, TIpcArgs());
}

EXPORT_C void RGOomMonitorSession::ThisAppIsNotExiting(TInt aWgId) {
    TIpcArgs p(aWgId);
    SendReceive(EGOomMonitorThisAppIsNotExiting, p);
}

EXPORT_C void RGOomMonitorSession::SetGOomPriority(TGOomPriority aPriority) {
}

EXPORT_C void RGOomMonitorSession::ApplicationAboutToStart(const TUid& aAppUid) {
    SendReceive(EGoomMonitorAppAboutToStart, TIpcArgs(aAppUid.iUid));
}
    
EXPORT_C void RGOomMonitorSession::UsesAbsoluteMemTargets(TBool aUseAbsoluteAmount) {
    SendReceive(EGoomMonitorAppUsesAbsoluteMemTargets, TIpcArgs(aUseAbsoluteAmount));
}

EXPORT_C TInt RGOomMonitorSession::Connect(TRequestStatus& aStatus) {
    return CreateSession(KGraphicsMemoryMonitorServerName, TVersion( 0, 0, 0 ), 1, EIpcSession_Unsharable, 0, &aStatus );
}

EXPORT_C void RGOomMonitorSession::AppAboutToStart(TRequestStatus& aStatus, const TUid& aAppUid) {
    SendReceive(EGoomMonitorAppAboutToStart, TIpcArgs(aAppUid.iUid), aStatus);
}

EXPORT_C void StubbedExp1() {

}

EXPORT_C void StubbedExp2() {

}

EXPORT_C void StubbedExp11() {

}

EXPORT_C void StubbedExp12() {

}

EXPORT_C void StubbedExp13() {

}

EXPORT_C void StubbedExp14() {

}

EXPORT_C void StubbedExp15() {

}

EXPORT_C void StubbedExp16() {

}

EXPORT_C void StubbedExp17() {

}

EXPORT_C void StubbedExp18() {

}

EXPORT_C void StubbedExp19() {

}

EXPORT_C void StubbedExp20() {

}

EXPORT_C void StubbedExp21() {

}

