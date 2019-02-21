#include <epoc/services/window/classes/plugins/animdll.h>
#include <epoc/services/window/op.h>

namespace eka2l1::epoc {
    void anim_dll::execute_command(service::ipc_context &ctx, ws_cmd cmd) {
        TWsAnimDllOpcode op = static_cast<decltype(op)>(cmd.header.op);

        switch (op) {
        case EWsAnimDllOpCreateInstance: {
            LOG_TRACE("AnimDll::CreateInstance stubbed with a anim handle (>= 0)");
            ctx.set_request_status(user_count++);

            break;
        }

        case EWsAnimDllOpCommandReply: {
            LOG_TRACE("AnimDll command reply stubbed!");
            ctx.set_request_status(KErrNone);

            break;
        }

        default: {
            LOG_ERROR("Unimplement AnimDll Opcode: 0x{:x}", cmd.header.op);
            break;
        }
        }
    }
}