#include <services/fontbitmap/fontbitmap.h>
#include <services/fontbitmap/op.h>

#include <common/log.h>

namespace eka2l1 {
    fontbitmap_server::fontbitmap_server(system *sys)
        : service::server(sys, "!Fontbitmapserver") {
        REGISTER_IPC(fontbitmap_server, init, EFbsMessInit,
            "FbsServer::Init");
    }

    void fontbitmap_server::init(service::ipc_context ctx) {
        ctx.set_request_status(0x40000000); // fake address for font bit map server
        LOG_INFO("FontBitmapServer::Init stubbed");
    }
}