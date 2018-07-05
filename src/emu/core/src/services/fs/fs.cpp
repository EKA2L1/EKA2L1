#include <services/fs/fs.h>
#include <services/fs/op.h>

#include <epoc/des.h>
#include <memory>

#include <common/cvt.h>
#include <common/path.h>
#include <common/log.h>

#include <common/e32inc.h>

#include <filesystem>
#include <vfs.h>

#include <core.h>

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

namespace eka2l1::epoc {
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
        REGISTER_IPC(fs_server, file_size, EFsFileSize, "Fs::FileSize");
        REGISTER_IPC(fs_server, file_seek, EFsFileSeek, "Fs::FileSeek");
        REGISTER_IPC(fs_server, file_read, EFsFileRead, "Fs::FileRead");
        REGISTER_IPC(fs_server, file_replace, EFsFileReplace, "Fs::FileReplace");
        REGISTER_IPC(fs_server, open_dir, EFsDirOpen, "Fs::OpenDir");
    }

    fs_node *fs_server::get_file_node(int handle) {
        int real_handle = file_handles.get_real_handle_id(handle);

        if (real_handle == -1) {
            return nullptr;
        }

        return &file_nodes[real_handle];
    }

    void fs_server::file_size(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        ctx.write_arg_pkg<uint64_t>(0, node->vfs_node->size());
        ctx.set_request_status(KErrNone);
    }

    void fs_server::file_seek(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        std::optional<int> seek_mode = ctx.get_arg<int>(1);
        std::optional<int> seek_off = ctx.get_arg<int>(0);

        if (!seek_mode || !seek_off) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        file_seek_mode vfs_seek_mode;

        switch (*seek_mode) {
        case 0: // ESeekAddress. Handle this as a normal seek start
            vfs_seek_mode = file_seek_mode::beg;
            break;

        default:
            vfs_seek_mode = static_cast<file_seek_mode>(*seek_mode - 1);
            break;
        }

        node->vfs_node->seek(*seek_off, vfs_seek_mode);
        ctx.write_arg_pkg(3, static_cast<int>(node->vfs_node->tell()));

        ctx.set_request_status(KErrNone);
    }

    void fs_server::file_read(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        if (!(node->open_mode & READ_MODE)) {
            ctx.set_request_status(KErrAccessDenied);
            return;
        }

        int read_len = *ctx.get_arg<int>(1);

        int read_pos = *ctx.get_arg<int>(2);
        uint64_t last_pos = node->vfs_node->tell();
        uint64_t size = node->vfs_node->size();

        if (size - read_pos < read_len) {
            read_len = size - last_pos;
        }

        node->vfs_node->seek(read_pos, file_seek_mode::beg);

        std::vector<char> read_data;
        read_data.resize(read_len);

        node->vfs_node->read_file(read_data.data(), 1, read_len);

        ctx.write_arg_pkg(0, reinterpret_cast<uint8_t *>(read_data.data()), read_len);
        ctx.set_request_status(read_len);
    }

    void fs_server::new_file_subsession(service::ipc_context ctx, bool overwrite) {
        std::optional<std::u16string> name_res = ctx.get_arg<std::u16string>(0);
        std::optional<int> open_mode_res = ctx.get_arg<int>(1);

        if (!name_res || !open_mode_res) {
            ctx.set_request_status(KErrArgument);
        }

        LOG_INFO("Opening file: {}", common::ucs2_to_utf8(*name_res));

        int handle = new_node(ctx.sys->get_io_system(), *name_res, *open_mode_res, overwrite);

        if (handle <= 0) {
            ctx.set_request_status(handle);
            return;
        }

        ctx.write_arg_pkg<int>(3, handle);
        ctx.set_request_status(KErrNone);
    }

    void fs_server::file_open(service::ipc_context ctx) {
        new_file_subsession(ctx);
    }

    void fs_server::file_replace(service::ipc_context ctx) {
        new_file_subsession(ctx, true);
    }

    int fs_server::new_node(io_system *io, std::u16string name, int org_mode, bool overwrite) {
        int real_mode = org_mode & ~(epoc::EFileStreamText | epoc::EFileReadAsyncAll | epoc::EFileBigFile);
        fs_node_share share_mode = (fs_node_share)-1;

        if (real_mode & epoc::EFileShareExclusive) {
            share_mode = fs_node_share::exclusive;
        } else if (real_mode & epoc::EFileShareReadersOnly) {
            share_mode = fs_node_share::share_read;
        } else if (real_mode & epoc::EFileShareReadersOrWriters) {
            share_mode = fs_node_share::share_read_write;
        } else if (real_mode & epoc::EFileShareAny) {
            share_mode = fs_node_share::any;
        }

        // Fetch open mode
        int access_mode = -1;

        if (!(real_mode & epoc::EFileStreamText)) {
            access_mode = BIN_MODE;
        } else {
            access_mode = 0;
        }

        if (real_mode & epoc::EFileWrite) {
            if (overwrite)
                access_mode |= WRITE_MODE;
            else
                access_mode |= APPEND_MODE;
        } else {
            // Since EFileRead = 0, they default to read mode if nothing is specified more
            access_mode |= READ_MODE;
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

            new_node.mix_mode = real_mode;
            new_node.open_mode = access_mode;
            new_node.share_mode = share_mode;
            new_node.id = file_handles.new_handle(static_cast<handle_owner_type>(owner_type),
                kern->get_id_base_owner(owner_type));

            uint32_t id = new_node.id;
            file_nodes.emplace(id, std::move(new_node));

            return id;
        }

        if ((int)share_mode != -1 && share_mode != cache_node->second.share_mode) {
            if (share_mode == fs_node_share::exclusive || cache_node->second.share_mode == fs_node_share::exclusive) {
                return KErrAccessDenied;
            }

            // Compare mode, compatible ?
            if ((share_mode == fs_node_share::share_read && cache_node->second.share_mode == fs_node_share::any)
                || (share_mode == fs_node_share::share_read && cache_node->second.share_mode == fs_node_share::share_read_write && (cache_node->second.open_mode & WRITE_MODE))
                || (share_mode == fs_node_share::share_read_write && (access_mode & WRITE_MODE) && cache_node->second.share_mode == fs_node_share::share_read)
                || (share_mode == fs_node_share::any && cache_node->second.share_mode == fs_node_share::share_read)) {
                return KErrAccessDenied;
            }

            // Let's promote mode

            // Since we filtered incompatible mode, so if the share mode of them two is read, share mode of both of them is read
            if (share_mode == fs_node_share::share_read || cache_node->second.share_mode == fs_node_share::share_read) {
                share_mode = fs_node_share::share_read;
                cache_node->second.share_mode = fs_node_share::share_read;
            }

            if (share_mode == fs_node_share::any || cache_node->second.share_mode == fs_node_share::any) {
                share_mode = fs_node_share::any;
                cache_node->second.share_mode = fs_node_share::any;
            }

            if (share_mode == fs_node_share::share_read_write || share_mode == fs_node_share::share_read_write) {
                share_mode = fs_node_share::share_read_write;
                cache_node->second.share_mode = fs_node_share::share_read_write;
            }
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

        new_node.mix_mode = real_mode;
        new_node.open_mode = access_mode;
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

    void fs_server::open_dir(service::ipc_context ctx) {
        ctx.set_request_status(KErrNotFound);
    }
}