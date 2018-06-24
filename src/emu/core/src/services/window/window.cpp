#include <services/window/op.h>
#include <services/window/window.h>

#include <core.h>

namespace eka2l1 {
    window_server::window_server(system *sys)
        : service::server(sys, "!Windowserver") {
        REGISTER_IPC(window_server, init, EWservMessInit,
            "Ws::Init");
    }

    void window_server::init(service::ipc_context ctx) {
        // Since request status is an int (retarded design), this would 
        // expect address lower then 0x80000000. Meaning that it would resides
        // in shared heap memory. These servers are virtual, so this will be a placeholder
        // context
        ctx.set_request_status(0x40000000);
    }
}