#pragma once

#include <core/services/server.h>

namespace eka2l1 {
    namespace epoc {
        struct ldr_info {
            uint32_t uid1;
            uint32_t uid2;
            uint32_t uid3;
            int owner_type;
            int handle;
            uint32_t secure_id;
            uint32_t requested_version;
            int min_stack_size;
        };
    }

    class loader_server : public service::server {
        /*! \brief Parse a E32 Image, and use informations from parsing to spawn a new process.
         * 
         * arg0: TLdrInfo contains spawn request
         * arg1: Contains path to the image.
         * arg2: Contains extra argument for the process.
         *
         * request_status: New Process handle.
        */
        void load_process(eka2l1::service::ipc_context context);

    public:
        loader_server(system *sys);
    };
}