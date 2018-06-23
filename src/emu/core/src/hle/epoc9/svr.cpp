#include <epoc9/svr.h>

#include <common/cvt.h>

BRIDGE_FUNC(TInt, RSessionBaseCreateSession, eka2l1::ptr<RSessionBase> aSession, eka2l1::ptr<TDesC> aServerName,
    eka2l1::ptr<void> aVersion, TInt aAsyncMsgSlot) {
    memory_system *mem = sys->get_memory_system();
    kernel_system *kern = sys->get_kernel_system();

    std::string server_name = common::ucs2_to_utf8(aServerName.get(mem)->StdString(sys));

    server_ptr svr = kern->get_server_by_name(server_name);

    if (!svr) {
        LOG_ERROR("Unexist server: {}", server_name);
        return KErrGeneral;
    }

    session_ptr nss = kern->create_session(svr, aAsyncMsgSlot);
    aSession.get(mem)->iHandle = nss->unique_id();

    LOG_INFO("Session created, connected to {} witht total of {} async slots.", server_name, aAsyncMsgSlot);

    return KErrNone;
}

BRIDGE_FUNC(TInt, RSessionBaseDoSendReceive, eka2l1::ptr<RSessionBase> aSession, TInt aFunction, eka2l1::ptr<TIpcArgs> aArgs) {
    memory_system *mem = sys->get_memory_system();
    kernel_system *kern = sys->get_kernel_system();

    TIpcArgs *ipc_args = aArgs.get(mem);

    session_ptr ss = kern->get_session(aSession.get(mem)->iHandle);

    if (!ss) {
        return KErrBadHandle;
    }

    return ss->send_receive_sync(aFunction, ipc_arg(ipc_args->iArg0, ipc_args->iArg1, ipc_args->iArg2, ipc_args->iArg3, ipc_args->iFlag));
}

const eka2l1::hle::func_map svr_register_funcs = {
    BRIDGE_REGISTER(3740794791, RSessionBaseCreateSession),
    BRIDGE_REGISTER(2407847085, RSessionBaseDoSendReceive)
};