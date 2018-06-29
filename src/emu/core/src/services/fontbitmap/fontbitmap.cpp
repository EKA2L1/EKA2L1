#include <services/fontbitmap/fontbitmap.h>
#include <services/fontbitmap/op.h>

#include <core.h>
#include <common/log.h>

namespace eka2l1 {
    fontbitmap_server::fontbitmap_server(system *sys)
        : service::server(sys, "!Fontbitmapserver") {
        REGISTER_IPC(fontbitmap_server, init, EFbsMessInit,
            "FbsServer::Init");
    }

    void fontbitmap_server::init(service::ipc_context ctx) {
        fbs_shared_chunk = ctx.sys->get_kernel_system()->create_chunk(
            "FbsSharedChunk", 0, 0x5000, 0x5000,
            prot::read_write, kernel::chunk_type::disconnected, kernel::chunk_access::global,
            kernel::chunk_attrib::none, kernel::owner_type::process);

        ctx.set_request_status(obj_id); 
        LOG_INFO("FontBitmapServer::Init stubbed (maybe)");
    }
}