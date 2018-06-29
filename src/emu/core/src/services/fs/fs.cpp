#include <services/fs/fs.h>
#include <services/fs/op.h>

#include <epoc9/des.h>
#include <memory>

#include <common/cvt.h>
#include <common/path.h>

#include <common/e32inc.h>

#include <filesystem>

namespace fs = std::experimental::filesystem;

const TUint KEntryAttNormal = 0x0000;
const TUint KEntryAttReadOnly = 0x0001;
const TUint KEntryAttHidden = 0x0002;
const TUint KEntryAttSystem = 0x0004;
const TUint KEntryAttVolume = 0x0008;
const TUint KEntryAttDir = 0x0010;
const TUint KEntryAttArchive = 0x0020;
const TUint KEntryAttXIP = 0x0080;
const TUint KEntryAttRemote = 0x0100;
const TUint KEntryAttMaskFileSystemSpecific = 0x00FF0000;
const TUint KEntryAttMatchMask = (KEntryAttHidden | KEntryAttSystem | KEntryAttDir);

namespace eka2l1 {
    fs_server::fs_server(system *sys)
        : service::server(sys, "!FileServer") {
        REGISTER_IPC(fs_server, entry, EFsEntry, "Fs::Entry");
    }

    bool is_e32img(symfile f) {
        int sig;

        f->seek(12, file_seek_mode::beg);
        f->read_file(&sig, 1, 4);

        if (sig != 0x434F5045) {
            return false;
        }

        return true;
    }

    void fs_server::entry(service::ipc_context ctx) {
        std::optional<std::u16string> fname_op = ctx.get_arg<std::u16string>(0);

        if (!fname_op) {
            ctx.set_request_status(KErrArgument);
        }

        std::string path = common::ucs2_to_utf8(*fname_op);

        LOG_INFO("Get entry of: {}", path);

        bool dir = false;

        io_system *io = ctx.sys->get_io_system();
        symfile file = io->open_file(*fname_op, READ_MODE | BIN_MODE);

        if (!file) {
            if (eka2l1::is_dir(io->get(path))) {
                dir = true;
            } else {
                ctx.set_request_status(KErrNotFound);
                return;
            }
        }

        epoc::TEntry entry;
        entry.aSize = dir ? 0 : file->size();

        if ((*fname_op)[0] == u'Z' || (*fname_op)[0] == u'z') {
            entry.aAttrib = KEntryAttReadOnly | KEntryAttSystem;

            if (!dir && (*fname_op).find(u".dll") && !is_e32img(file)) {
                entry.aAttrib |= KEntryAttXIP;
            }
        }

        if (dir) {
            entry.aAttrib |= KEntryAttDir;
        } else {
            entry.aAttrib |= KEntryAttNormal;
        }

        entry.aNameLength = (*fname_op).length();
        entry.aSizeHigh = 0; // This is never used, since the size is never >= 4GB as told by Nokia Doc

        memcpy(entry.aName, (*fname_op).data(), entry.aNameLength * 2);

        auto last_mod = fs::last_write_time(io->get(path));
   
        entry.aModified = epoc::TTime{static_cast<uint64_t>(last_mod.time_since_epoch().count())};
        ctx.write_arg_pkg<epoc::TEntry>(1, entry);

        ctx.set_request_status(KErrNone);
    }
}