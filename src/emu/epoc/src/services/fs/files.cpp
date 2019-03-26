/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <epoc/services/fs/fs.h>
#include <epoc/services/fs/std.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/vfs.h>

#include <common/e32inc.h>
#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/random.h>
#include <e32err.h>

namespace eka2l1 {
    void fs_server_client::file_size(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        // On Symbian^3 onwards, 64-bit file were supported, 64-bit integer for filesize used by default
        if (ctx.sys->get_kernel_system()->get_epoc_version() >= epocver::epoc10) {
            std::uint64_t fsize_u = std::reinterpret_pointer_cast<file>(node->vfs_node)->size();
            ctx.write_arg_pkg<uint64_t>(0, fsize_u);
        } else {
            std::uint32_t fsize_u = static_cast<std::uint32_t>(std::reinterpret_pointer_cast<file>(node->vfs_node)->size());
            ctx.write_arg_pkg<uint32_t>(0, fsize_u);
        }

        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_set_size(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        if (!(node->open_mode & WRITE_MODE) && !(node->open_mode & APPEND_MODE)) {
            // Can't set file size if the file is not open for write
            ctx.set_request_status(KErrPermissionDenied);
            return;
        }

        int size = *ctx.get_arg<int>(0);
        symfile f = std::reinterpret_pointer_cast<file>(node->vfs_node);
        std::size_t fsize = f->size();

        if (size == fsize) {
            ctx.set_request_status(KErrNone);
            return;
        }

        // This is trying to prevent from data corruption that will affect the host
        if (size >= common::GB(1)) {
            LOG_ERROR("File trying to resize to 1GB, operation not permitted");
            ctx.set_request_status(KErrTooBig);

            return;
        }

        bool res = f->resize(size);

        if (!res) {
            ctx.set_request_status(KErrGeneral);
            return;
        }

        // If the file is truncated, move the file pointer to the maximum new size
        if (size < fsize) {
            f->seek(size, file_seek_mode::beg);
        }

        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_name(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        symfile f = std::reinterpret_pointer_cast<file>(node->vfs_node);

        ctx.write_arg(0, eka2l1::filename(f->file_name(), true));
        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_full_name(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        symfile f = std::reinterpret_pointer_cast<file>(node->vfs_node);

        ctx.write_arg(0, f->file_name());
        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_seek(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        symfile vfs_file = std::reinterpret_pointer_cast<file>(node->vfs_node);

        std::optional<int> seek_mode = ctx.get_arg<int>(1);
        std::optional<int> seek_off = ctx.get_arg<int>(0);

        if (!seek_mode || !seek_off) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        file_seek_mode vfs_seek_mode;

        switch (*seek_mode) {
        case 0: // ESeekAddress. Handle this as a normal seek start
            vfs_seek_mode = file_seek_mode::address;
            break;

        default:
            vfs_seek_mode = static_cast<file_seek_mode>(*seek_mode - 1);
            break;
        }

        // This should also support negative
        std::uint64_t seek_res = vfs_file->seek(*seek_off, vfs_seek_mode);

        if (seek_res == 0xFFFFFFFFFFFFFFFF) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        // Slot order: (0) seek offset, (1) seek mode, (2) new pos

        if ((int)ctx.sys->get_symbian_version_use() >= (int)epocver::epoc10) {
            ctx.write_arg_pkg(2, seek_res);
        } else {
            int seek_result_i = static_cast<TInt>(seek_res);
            ctx.write_arg_pkg(2, seek_result_i);
        }

        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_flush(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        symfile vfs_file = std::reinterpret_pointer_cast<file>(node->vfs_node);

        if (!vfs_file->flush()) {
            ctx.set_request_status(KErrGeneral);
            return;
        }

        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_rename(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        symfile vfs_file = std::reinterpret_pointer_cast<file>(node->vfs_node);
        auto new_path = ctx.get_arg<std::u16string>(0);

        if (!new_path) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        auto new_path_abs = eka2l1::absolute_path(*new_path, ss_path);
        bool res = ctx.sys->get_io_system()->rename(vfs_file->file_name(), new_path_abs);

        if (!res) {
            ctx.set_request_status(KErrGeneral);
            return;
        }

        // Save state of file and reopening it
        size_t last_pos = vfs_file->tell();
        int last_mode = vfs_file->file_mode();

        vfs_file->close();

        vfs_file = ctx.sys->get_io_system()->open_file(new_path_abs, last_mode);
        vfs_file->seek(last_pos, file_seek_mode::beg);

        node->vfs_node = std::move(vfs_file);

        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_write(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::optional<std::string> write_data = ctx.get_arg<std::string>(0);

        if (!write_data) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        symfile vfs_file = std::reinterpret_pointer_cast<file>(node->vfs_node);

        if (!(node->open_mode & WRITE_MODE)) {
            ctx.set_request_status(KErrAccessDenied);
            return;
        }

        int write_len = *ctx.get_arg<int>(1);
        int write_pos_provided = *ctx.get_arg<int>(2);

        std::uint64_t write_pos = 0;
        std::uint64_t last_pos = vfs_file->tell();
        bool should_reseek = false;

        write_pos = last_pos;

        // Low MaxUint64
        if (write_pos_provided != static_cast<int>(0x80000000)) {
            write_pos = write_pos_provided;
        }

        // If this write pos is beyond the current end of file, use last pos
        vfs_file->seek(write_pos > last_pos ? last_pos : write_pos, file_seek_mode::beg);
        size_t wrote_size = vfs_file->write_file(&(*write_data)[0], 1, write_len);

        LOG_TRACE("File {} wroted with size: {}",
            common::ucs2_to_utf8(vfs_file->file_name()), wrote_size);

        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_read(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        symfile vfs_file = std::reinterpret_pointer_cast<file>(node->vfs_node);

        if (!(node->open_mode & READ_MODE)) {
            ctx.set_request_status(KErrAccessDenied);
            return;
        }

        int read_len = *ctx.get_arg<int>(1);
        int read_pos_provided = *ctx.get_arg<int>(2);

        std::uint64_t read_pos = 0;
        std::uint64_t last_pos = vfs_file->tell();
        bool should_reseek = false;

        read_pos = last_pos;

        // Low MaxUint64
        if (read_pos_provided != static_cast<int>(0x80000000)) {
            read_pos = read_pos_provided;
        }

        vfs_file->seek(read_pos, file_seek_mode::beg);

        uint64_t size = vfs_file->size();

        if (size - read_pos < read_len) {
            read_len = static_cast<int>(size - last_pos);
        }

        std::vector<char> read_data;
        read_data.resize(read_len);

        size_t read_finish_len = vfs_file->read_file(read_data.data(), 1, read_len);

        ctx.write_arg_pkg(0, reinterpret_cast<uint8_t *>(read_data.data()), read_len);

        LOG_TRACE("Readed {} from {} to address 0x{:x}", read_finish_len, read_pos, ctx.msg->args.args[0]);
        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_close(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        symfile vfs_file = std::reinterpret_pointer_cast<file>(node->vfs_node);
        std::u16string path = vfs_file->file_name();

        vfs_file->close();

        // Delete temporary file
        if (node->temporary) {
            ctx.sys->get_io_system()->delete_entry(path);
        }

        nodes_table.close_nodes(*handle_res);
        ctx.set_request_status(KErrNone);
    }
    
    void fs_server_client::file_open(service::ipc_context ctx) {
        std::optional<std::u16string> name_res = ctx.get_arg<std::u16string>(0);
        std::optional<int> open_mode_res = ctx.get_arg<int>(1);

        if (!name_res || !open_mode_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        // LOG_TRACE("Opening exist {}", common::ucs2_to_utf8(*name_res));

        *name_res = eka2l1::absolute_path(*name_res, ss_path);

        // Don't open file if it doesn't exist
        if (!ctx.sys->get_io_system()->exist(*name_res)) {
            LOG_TRACE("IO component not exists: {}", common::ucs2_to_utf8(*name_res));

            ctx.set_request_status(KErrNotFound);
            return;
        }

        new_file_subsession(ctx);
    }

    void fs_server_client::file_replace(service::ipc_context ctx) {
        new_file_subsession(ctx, true);
    }

    void fs_server_client::file_temp(service::ipc_context ctx) {
        auto dir_create = ctx.get_arg<std::u16string>(0);

        if (!dir_create) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        io_system *io = ctx.sys->get_io_system();

        auto full_path = eka2l1::absolute_path(*dir_create, ss_path);

        if (!io->exist(full_path)) {
            LOG_TRACE("Directory for temp file not exists", common::ucs2_to_utf8(full_path));

            ctx.set_request_status(KErrPathNotFound);
            return;
        }

        std::u16string temp_name{ u"temp" };
        temp_name += common::utf8_to_ucs2(common::to_string(eka2l1::random_range(0, 0xFFFFFFFE), std::hex));

        full_path = eka2l1::add_path(full_path, temp_name);

        // Create the file if it doesn't exist
        symfile f = io->open_file(full_path, WRITE_MODE);
        f->close();

        LOG_INFO("Opening temp file: {}", common::ucs2_to_utf8(full_path));
        int handle = new_node(ctx.sys->get_io_system(), ctx.msg->own_thr, full_path,
            *ctx.get_arg<int>(1), true, true);

        if (handle <= 0) {
            ctx.set_request_status(handle);
            return;
        }

        LOG_TRACE("Handle opended: {}", handle);

        ctx.write_arg_pkg<int>(3, handle);

        // Arg2 take the temp path
        ctx.write_arg(2, full_path);

        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_create(service::ipc_context ctx) {
        std::optional<std::u16string> name_res = ctx.get_arg<std::u16string>(0);
        std::optional<int> open_mode_res = ctx.get_arg<int>(1);

        if (!name_res || !open_mode_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        // If the file already exist, stop
        if (ctx.sys->get_io_system()->exist(*name_res)) {
            ctx.set_request_status(KErrAlreadyExists);
            return;
        }

        new_file_subsession(ctx, true);
    }

    void fs_server_client::file_duplicate(service::ipc_context ctx) {
        int target_handle = *ctx.get_arg<int>(0);
        fs_node *node = nodes_table.get_node(target_handle);

        if (!node) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        int dup_handle = static_cast<int>(nodes_table.add_node(*node));

        ctx.write_arg_pkg<int>(3, dup_handle);
        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::file_adopt(service::ipc_context ctx) {
        LOG_TRACE("Fs::FileAdopt stubbed");
        // TODO (pent0) : Do an adopt implementation

        ctx.set_request_status(KErrNone);
    }
    
    void fs_server_client::new_file_subsession(service::ipc_context ctx, bool overwrite, bool temporary) {
        std::optional<std::u16string> name_res = ctx.get_arg<std::u16string>(0);
        std::optional<int> open_mode_res = ctx.get_arg<int>(1);

        if (!name_res || !open_mode_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        *name_res = eka2l1::absolute_path(*name_res, ss_path);
        std::string name_utf8 = common::ucs2_to_utf8(*name_res);

        {
            auto file_dir = eka2l1::file_directory(*name_res);

            // Do a check to return KErrPathNotFound
            if (!ctx.sys->get_io_system()->exist(file_dir)) {
                LOG_TRACE("Base directory of file {} not found", name_utf8);

                ctx.set_request_status(KErrPathNotFound);
                return;
            }
        }

        LOG_INFO("Opening file: {}", name_utf8);

        int handle = new_node(ctx.sys->get_io_system(), ctx.msg->own_thr, *name_res,
            *open_mode_res, overwrite, temporary);

        if (handle <= 0) {
            ctx.set_request_status(handle);
            return;
        }

        LOG_TRACE("Handle opended: {}", handle);

        ctx.write_arg_pkg<int>(3, handle);
        ctx.set_request_status(KErrNone);
    }
    
    int fs_server_client::new_node(io_system *io, thread_ptr sender, std::u16string name, int org_mode, bool overwrite, bool temporary) {
        int real_mode = org_mode & ~(epoc::fs::file_stream_text | epoc::fs::file_read_async_all | 
            epoc::fs::file_big_size);

        fs_node_share share_mode = (fs_node_share)-1;

        if (real_mode & epoc::fs::file_share_exclusive) {
            share_mode = fs_node_share::exclusive;
        } else if (real_mode & epoc::fs::file_share_readers_only) {
            share_mode = fs_node_share::share_read;
        } else if (real_mode & epoc::fs::file_share_readers_or_writers) {
            share_mode = fs_node_share::share_read_write;
        } else if (real_mode & epoc::fs::file_share_any) {
            share_mode = fs_node_share::any;
        }

        // Fetch open mode
        int access_mode = -1;

        if (!(real_mode & epoc::fs::file_stream_text)) {
            access_mode = BIN_MODE;
        } else {
            access_mode = 0;
        }

        if (real_mode & epoc::fs::file_write) {
            if (overwrite) {
                access_mode |= WRITE_MODE;
            } else {
                access_mode |= APPEND_MODE;
            }
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

        fs_node *cache_node = nodes_table.get_node(name);

        if (!cache_node) {
            fs_node new_node;
            new_node.vfs_node = io->open_file(name, access_mode);
            new_node.temporary = temporary;

            if (!new_node.vfs_node) {
                LOG_TRACE("Can't open file {}", common::ucs2_to_utf8(name));
                return KErrNotFound;
            }

            if ((int)share_mode == -1) {
                share_mode = fs_node_share::share_read_write;
            }

            new_node.mix_mode = real_mode;
            new_node.open_mode = access_mode;
            new_node.share_mode = share_mode;
            new_node.own_process = sender->owning_process();

            std::size_t h = nodes_table.add_node(new_node);
            return (h == 0) ? KErrNoMemory : static_cast<int>(h);
        }

        if ((int)share_mode != -1 && share_mode != cache_node->share_mode) {
            if (share_mode == fs_node_share::exclusive || cache_node->share_mode == fs_node_share::exclusive) {
                return KErrAccessDenied;
            }

            // Compare mode, compatible ?
            if ((share_mode == fs_node_share::share_read && cache_node->share_mode == fs_node_share::any)
                || (share_mode == fs_node_share::share_read && cache_node->share_mode == fs_node_share::share_read_write && (cache_node->open_mode & WRITE_MODE))
                || (share_mode == fs_node_share::share_read_write && (access_mode & WRITE_MODE) && cache_node->share_mode == fs_node_share::share_read)
                || (share_mode == fs_node_share::any && cache_node->share_mode == fs_node_share::share_read)) {
                return KErrAccessDenied;
            }

            // Let's promote mode

            // Since we filtered incompatible mode, so if the share mode of them two is read, share mode of both of them is read
            if (share_mode == fs_node_share::share_read || cache_node->share_mode == fs_node_share::share_read) {
                share_mode = fs_node_share::share_read;
                cache_node->share_mode = fs_node_share::share_read;
            }

            if (share_mode == fs_node_share::any || cache_node->share_mode == fs_node_share::any) {
                share_mode = fs_node_share::any;
                cache_node->share_mode = fs_node_share::any;
            }

            if (share_mode == fs_node_share::share_read_write || share_mode == fs_node_share::share_read_write) {
                share_mode = fs_node_share::share_read_write;
                cache_node->share_mode = fs_node_share::share_read_write;
            }
        } else {
            share_mode = cache_node->share_mode;
        }

        if (share_mode == fs_node_share::share_read && access_mode & WRITE_MODE) {
            return KErrAccessDenied;
        }

        // Check if mode is compatible
        if (cache_node->share_mode == fs_node_share::exclusive) {
            // Check if process is the same
            // Deninded if mode is exclusive
            if (cache_node->own_process != sender->owning_process()) {
                LOG_TRACE("File open mode is exclusive, denide access");
                return KErrAccessDenied;
            }
        }

        // If we have the same open mode as the cache node, don't create new, returns this :D
        if (cache_node->open_mode == real_mode) {
            return cache_node->id;
        }

        fs_node new_node;
        new_node.vfs_node = io->open_file(name, access_mode);

        if (!new_node.vfs_node) {
            LOG_TRACE("Can't open file {}", common::ucs2_to_utf8(name));
            return KErrNotFound;
        }

        new_node.mix_mode = real_mode;
        new_node.open_mode = access_mode;
        new_node.share_mode = share_mode;

        std::size_t h = nodes_table.add_node(new_node);
        return (h == 0) ? KErrNoMemory : static_cast<int>(h);
    }
}
