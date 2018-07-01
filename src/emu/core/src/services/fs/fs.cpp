#include <services/fs/fs.h>
#include <services/fs/op.h>

#include <epoc9/des.h>
#include <memory>

#include <common/cvt.h>
#include <common/path.h>

#include <common/e32inc.h>

#include <filesystem>
#include <vfs.h>

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

namespace epoc {
    enum TFileMode {
        EFileShareExclusive,
        EFileShareReadersOnly,
        EFileShareAny,
        EFileShareReadersOrWriters,
        EFileStream = 0,
        EFileStreamText = 0x100,
        EFileRead = 0,
        EFileWrite = 0x200,
        EFileReadAsyncAll = 0x400,
        EFileWriteBuffered = 0x00000800,
        EFileWriteDirectIO = 0x00001000,
        EFileReadBuffered = 0x00002000,
        EFileReadDirectIO = 0x00004000,
        EFileReadAheadOn = 0x00008000,
        EFileReadAheadOff = 0x00010000,
        EDeleteOnClose = 0x00020000,
        EFileBigFile = 0x00040000,
        EFileSequential = 0x00080000
    };
}

namespace eka2l1 {
    fs_server::fs_server(system *sys)
        : service::server(sys, "!FileServer") {
        REGISTER_IPC(fs_server, entry, EFsEntry, "Fs::Entry");
        REGISTER_IPC(fs_server, file_open, EFsFileOpen, "Fs::FileOpen");
    }

    void fs_server::file_open(service::ipc_context ctx) {
        std::optional<std::u16string> name_res = ctx.get_arg<std::u16string>(0);
        std::optional<int> open_mode_res = ctx.get_arg<int>(1);

        if (!name_res || !open_mode_res) {
            ctx.set_request_status(KErrArgument);
        }

        uint32_t handle = new_node(ctx.sys->get_io_system(), *name_res, *open_mode_res);

        if (handle <= 0) {
            ctx.set_request_status(KErrNotFound);
        }

        ctx.write_arg_pkg<uint32_t>(3, handle);
        ctx.set_request_status(KErrNone);
    }

    int fs_server::new_node(io_system *io, std::u16string name, int org_mode) {
        int real_mode = org_mode & ~(epoc::EFileStreamText | epoc::EFileReadAsyncAll | epoc::EFileBigFile);
        fs_node_share share_mode = (fs_node_share)-1;

        if (real_mode & epoc::EFileShareExclusive) {
            share_mode = fs_node_share::exclusive;
        } else if (real_mode & epoc::EFileShareReadersOnly) {
            share_mode = fs_node_share::share_read;
        } else if (real_mode & epoc::EFileShareReadersOrWriters || real_mode & epoc::EFileShareAny) {
            share_mode = fs_node_share::share_read_write;
        }

        // Fetch open mode
        int access_mode = -1;

        if (real_mode & epoc::EFileStream) {
            access_mode = BIN_MODE;
        } else {
            access_mode = 0;
        }

        if (real_mode & epoc::EFileRead) {
            access_mode = READ_MODE;
        } else if (real_mode & epoc::EFileWrite) {
            access_mode = WRITE_MODE;
        }

        if (access_mode & WRITE_MODE && share_mode == fs_node_share::share_read) {
            return KErrAccessDenied;
        }

        kernel::owner_type owner_type = kernel::owner_type::kernel;

        // Fetch owner
        if (share_mode == fs_node_share::exclusive) {
            owner_type = kernel::owner_type::process;
        }

        auto &cache_node = std::find_if(file_nodes.begin(), file_nodes.end(),
            [&](const auto &node) { return node.second.vfs_node->file_name() == name; });

        if (cache_node == file_nodes.end()) {
            fs_node new_node;
            new_node.vfs_node = io->open_file(name, access_mode);

            if (!new_node.vfs_node) {
                return KErrNotFound;
            }

            if ((int)share_mode == -1) {
                share_mode = fs_node_share::share_read_write;
            }

            new_node.mix_mode = org_mode;
            new_node.open_mode = real_mode;
            new_node.share_mode = share_mode;
            new_node.id = file_handles.new_handle(static_cast<handle_owner_type>(owner_type),
                kern->get_id_base_owner(owner_type));

            uint32_t id = new_node.id;
            file_nodes.emplace(id, std::move(new_node));

            return id;
        }

        if ((int)share_mode != -1 && share_mode != cache_node->second.share_mode) {
            return KErrAccessDenied;
        } else {
            share_mode = cache_node->second.share_mode;
        }

        if (share_mode == fs_node_share::share_read && access_mode & WRITE_MODE) {
            return KErrAccessDenied;
        }

        // Check if mode is compatible        
        if (cache_node->second.share_mode == fs_node_share::exclusive) {
            // Check if process id is the same
            uint64_t owner_id = file_handles.get_owner_id(cache_node->second.id);

            // Deninded if mode is exclusive
            if (owner_id != kern->get_id_base_owner(kernel::owner_type::process)) {
                return KErrAccessDenied;
            }
        }

        // If we have the same open mode as the cache node, don't create new, mirror the id
        if (cache_node->second.open_mode == real_mode) {
            uint32_t mirror_id = file_handles.new_handle(cache_node->second.id, static_cast<handle_owner_type>(owner_type),
                kern->get_id_base_owner(owner_type));

            return mirror_id;
        } 

        fs_node new_node;
        new_node.vfs_node = io->open_file(name, access_mode);

        if (!new_node.vfs_node) {
            return KErrNotFound;
        }

        new_node.mix_mode = org_mode;
        new_node.open_mode = real_mode;
        new_node.share_mode = share_mode;
        new_node.id = file_handles.new_handle(static_cast<handle_owner_type>(owner_type),
            kern->get_id_base_owner(owner_type));

        uint32_t id = new_node.id;
        file_nodes.emplace(id, std::move(new_node));

        return id;
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

        entry.aModified = epoc::TTime{ static_cast<uint64_t>(last_mod.time_since_epoch().count()) };
        ctx.write_arg_pkg<epoc::TEntry>(1, entry);

        ctx.set_request_status(KErrNone);
    }
}