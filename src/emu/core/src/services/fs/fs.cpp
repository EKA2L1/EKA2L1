#include <services/fs/fs.h>
#include <services/fs/op.h>

#include <memory>
#include <epoc9/des.h>

#include <common/cvt.h>

namespace eka2l1 {
    fs_server::fs_server(system *sys)
        : service::server(sys, "!FileServer") {
        REGISTER_IPC(fs_server, entry, EFsEntry, "Fs::Entry");
    }

    void fs_server::entry(service::ipc_context ctx) {
        std::optional<epoc::TEntry> fentry_op = ctx.get_arg_packed<epoc::TEntry>(1);
        std::optional<std::u16string> fname_op = ctx.get_arg<std::u16string>(0);

        LOG_INFO("Get entry of: {}", common::ucs2_to_utf8(*fname_op));

        ctx.set_request_status(0);
    }
}