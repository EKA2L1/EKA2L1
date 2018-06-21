#include <epoc9/svr.h>

BRIDGE_FUNC(TInt, RSessionBaseCreateSession, eka2l1::ptr<RSessionBase> aSession, eka2l1::ptr<TDesC> aServerName,
    eka2l1::ptr<void> aVersion, TInt aAsyncMsgSlot) {
    std::u16string server_name = aServerName.get(sys->get_memory_system())->StdString(sys);

    return KErrNone;
}

const eka2l1::hle::func_map svr_register_funcs = {
    BRIDGE_REGISTER(3740794791, RSessionBaseCreateSession)
};