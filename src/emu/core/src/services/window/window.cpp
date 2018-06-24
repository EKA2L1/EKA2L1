#include <services/window/op.h>
#include <services/window/window.h>

#include <common/log.h>

#include <core.h>
#include <optional>
#include <string>

namespace eka2l1 {
    window_server::window_server(system *sys)
        : service::server(sys, "!Windowserver") {
        REGISTER_IPC(window_server, init, EWservMessInit,
            "Ws::Init");
        REGISTER_IPC(window_server, parse_command_buffer, EWservMessCommandBuffer,
            "Ws::CommandBuffer");
        REGISTER_IPC(window_server, parse_command_buffer, EWservMessSyncMsgBuf,
            "Ws::MessSyncBuf");
    }

    void window_server::init(service::ipc_context ctx) {
        // Since request status is an int (retarded design), this would
        // expect address lower then 0x80000000. Meaning that it would resides
        // in shared heap memory. These servers are virtual, so this will be a placeholder
        // context
        ctx.set_request_status(0x40000000);
    }

    void window_server::parse_command_buffer(service::ipc_context ctx) {
        std::optional<std::string> dat = ctx.get_arg<std::string>(0);

        if (!dat) {
            return;
        }

        char *beg = dat->data();
        char *end = dat->data() + dat->size();

        std::vector<ws_cmd> cmds;

        while (beg < end) {
            ws_cmd cmd;

            cmd.header = *reinterpret_cast<ws_cmd_header *>(beg);
            beg += sizeof(ws_cmd_header);
            cmd.data_ptr = reinterpret_cast<void *>(beg);
            beg += cmd.header.cmd_len;

            if (cmd.header.op & 0x8000) {
                cmd.header.op &= ~0x8000;
            }

             cmds.push_back(std::move(cmd));
        }

        execute_commands(ctx, std::move(cmds));

        ctx.set_request_status(0);
    }

    void window_server::execute_commands(service::ipc_context ctx, std::vector<ws_cmd> cmds) {
        for (const auto &cmd : cmds) {
            execute_command(ctx, cmd);
        }
    }

    void window_server::execute_command(service::ipc_context ctx, ws_cmd cmd) {
        switch (cmd.header.op) {
        case EWsClOpCreateScreenDevice:
            LOG_INFO("WsCl::CreateScreenDevice: unimplemented sub-IPC call!");
            break;

        case EWsClOpCreateWindowGroup:
            LOG_INFO("WsCl::CreateWindowGroup: unimplemented sub-IPC call!");
            break;

        default:
            LOG_INFO("Unimplemented ClOp: 0x{:x}", cmd.header.op);
        }
    }
}