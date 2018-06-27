#include <services/fs/fs.h>
#include <services/fs/op.h>

#include <memory>
#include <epoc9/des.h>

namespace eka2l1 {
    fs_server::fs_server(system *sys)
        : service::server(sys, "!FileServer") {
        REGISTER_IPC(fs_server, entry, EFsEntry, "Fs::Entry");
    }

    void fs_server::entry(service::ipc_context ctx) {
        auto lol = ctx.get_arg<std::string>(1);
        LOG_INFO("Sizeof: 0x{:x}", sizeof(epoc::TEntry));
        std::optional<epoc::TEntry> fentry_op = ctx.get_arg_packed<epoc::TEntry>(1);
        std::optional<std::u16string> fname_op = ctx.get_arg<std::u16string>(0);

        ctx.set_request_status(0);
    }
}