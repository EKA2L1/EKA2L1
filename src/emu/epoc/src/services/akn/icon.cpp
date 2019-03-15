#include <epoc/services/akn/icon.h>

#include <common/e32inc.h>
#include <e32err.h>

namespace eka2l1 {
    akn_icon_server_session::akn_icon_server_session(service::typical_server *svr, service::uid client_ss_uid) 
        : service::typical_session(svr, client_ss_uid) {
    }

    void akn_icon_server_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        default: {
            LOG_ERROR("Unimplemented IPC opcode for AknIconServer session: 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }

    akn_icon_server::akn_icon_server(eka2l1::system *sys)
        : service::typical_server(sys, "!AknIconServer") {
    }
    
    void akn_icon_server::connect(service::ipc_context context) {
        create_session<akn_icon_server_session>(&context);
        context.set_request_status(KErrNone);
    }
}
