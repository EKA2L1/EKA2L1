#pragma once

#include <services/server.h>
#include <vector>

namespace eka2l1 {
    struct ws_cmd_header {
        uint16_t op;
        uint16_t cmd_len;
        uint32_t session_handle;
    };

    struct ws_cmd {
        ws_cmd_header header;
        void *data_ptr;
    };

    class window_server : public service::server {
        void execute_command(service::ipc_context ctx, ws_cmd cmd);
        void execute_commands(service::ipc_context ctx, std::vector<ws_cmd> cmds);

        void init(service::ipc_context ctx);
        void parse_command_buffer(service::ipc_context ctx);

    public:
        window_server(system *sys);
    };
}