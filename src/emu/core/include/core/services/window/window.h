#pragma once

#include <services/server.h>

namespace eka2l1 {
    class window_server: public service::server {
        void init(service::ipc_context ctx);
    public:
        window_server(system *sys);
    };
}