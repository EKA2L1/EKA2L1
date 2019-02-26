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

#include <e32err.h>

namespace eka2l1 {
    void fs_server_client::open_dir(service::ipc_context ctx) {
        auto dir = ctx.get_arg<std::u16string>(0);

        LOG_TRACE("Opening directory: {}", common::ucs2_to_utf8(*dir));

        if (!dir) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        const int attrib_raw = *ctx.get_arg<int>(1);
        io_attrib attrib = io_attrib::none;

        if (attrib_raw & epoc::fs::entry_att_dir) {
            attrib = attrib | io_attrib::include_dir;
        }

        fs_node node;
        node.vfs_node = ctx.sys->get_io_system()->open_dir(*dir, attrib);

        if (!node.vfs_node) {
            ctx.set_request_status(KErrPathNotFound);
            return;
        }

        node.own_process = ctx.msg->own_thr->owning_process();
        size_t dir_handle = nodes_table.add_node(node);

        struct uid_type {
            int uid[3];
        };

        uid_type type = *ctx.get_arg_packed<uid_type>(2);

        LOG_TRACE("UID requested: 0x{}, 0x{}, 0x{}", type.uid[0], type.uid[1], type.uid[2]);

        ctx.write_arg_pkg<int>(3, static_cast<int>(dir_handle));
        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::close_dir(service::ipc_context ctx) {
        std::optional<int> handle_res = ctx.get_arg<int>(3);

        if (!handle_res) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);

        if (node == nullptr || node->vfs_node->type != io_component_type::dir) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        nodes_table.close_nodes(*handle_res);
        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::read_dir(service::ipc_context ctx) {
        std::optional<int> handle = ctx.get_arg<int>(3);
        std::optional<int> entry_arr_vir_ptr = ctx.get_arg<int>(0);

        if (!handle || !entry_arr_vir_ptr) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *dir_node = nodes_table.get_node(*handle);

        if (!dir_node || dir_node->vfs_node->type != io_component_type::dir) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        std::shared_ptr<directory> dir = std::reinterpret_pointer_cast<directory>(
            dir_node->vfs_node);

        epoc::fs::entry entry;
        std::optional<entry_info> info = dir->get_next_entry();

        if (!info) {
            ctx.set_request_status(KErrEof);
            return;
        }

        if (info->has_raw_attribute) {
            entry.attrib = info->raw_attribute;
        } else {
            switch (info->attribute) {
            case io_attrib::hidden: {
                entry.attrib = epoc::fs::entry_att_hidden;
                break;
            }

            default:
                break;
            }

            if (info->type == io_component_type::dir) {
                entry.attrib &= epoc::fs::entry_att_dir;
            } else {
                entry.attrib &= epoc::fs::entry_att_archive;
            }
        }

        entry.size = static_cast<std::uint32_t>(info->size);
        entry.name = common::utf8_to_ucs2(info->full_path);
        entry.modified = epoc::time { info->last_write };

        ctx.set_request_status(KErrNone);
    }

    void fs_server_client::read_dir_packed(service::ipc_context ctx) {
        std::optional<int> handle = ctx.get_arg<int>(3);
        std::optional<int> entry_arr_vir_ptr = ctx.get_arg<int>(0);

        if (!handle || !entry_arr_vir_ptr) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        fs_node *dir_node = nodes_table.get_node(*handle);

        if (!dir_node || dir_node->vfs_node->type != io_component_type::dir) {
            ctx.set_request_status(KErrBadHandle);
            return;
        }

        std::shared_ptr<directory> dir = std::reinterpret_pointer_cast<directory>(dir_node->vfs_node);

        process_ptr own_pr = ctx.msg->own_thr->owning_process();

        epoc::des8 *entry_arr = ptr<epoc::des8>(*entry_arr_vir_ptr).get(own_pr);
        epoc::buf_des<char> *entry_arr_buf = reinterpret_cast<epoc::buf_des<char> *>(entry_arr);

        TUint8 *entry_buf = reinterpret_cast<TUint8 *>(entry_arr->get_pointer(own_pr));
        TUint8 *entry_buf_end = entry_buf + entry_arr_buf->max_length;
        TUint8 *entry_buf_org = entry_buf;

        size_t queried_entries = 0;

        // 4 is for info (length + descriptor type)
        size_t entry_no_name_size = offsetof(epoc::fs::entry, name) + 4 + 8;

        kernel_system *kern = ctx.sys->get_kernel_system();
        const bool should_support_64bit_size = kern->get_epoc_version() >= epocver::epoc10;

        while (entry_buf < entry_buf_end) {
            epoc::fs::entry entry;
            std::optional<entry_info> info = dir->peek_next_entry();

            if (!info) {
                entry_arr->set_length(own_pr, static_cast<std::uint32_t>(entry_buf - entry_buf_org));

                ctx.set_request_status(KErrEof);

                return;
            }

            if (entry_buf + entry_no_name_size + common::align(common::utf8_to_ucs2(info->name).length() * 2, 4) + 4 > entry_buf_end) {
                break;
            }

            if (info->has_raw_attribute) {
                entry.attrib = info->raw_attribute;
            } else {
                switch (info->attribute) {
                case io_attrib::hidden: {
                    entry.attrib = epoc::fs::entry_att_hidden;
                    break;
                }

                default:
                    break;
                }

                if (info->type == io_component_type::dir) {
                    entry.attrib &= epoc::fs::entry_att_dir;
                } else {
                    entry.attrib &= epoc::fs::entry_att_archive;
                }
            }

            entry.size = static_cast<std::uint32_t>(info->size);
            entry.name = common::utf8_to_ucs2(info->name);
            entry.modified = epoc::time { info->last_write };

            const std::uint32_t entry_write_size = offsetof(epoc::fs::entry, name) + 4;

            memcpy(entry_buf, &entry, entry_write_size);
            entry_buf += entry_write_size;

            memcpy(entry_buf, &entry.name.data[0], info->name.length());
            entry_buf += common::align(info->name.length() * 2, 4);

            if (should_support_64bit_size) {
                // Epoc10 uses two reserved bytes
                memcpy(entry_buf, &entry.size_high, 8);
                entry_buf += 8;
            }

            queried_entries += 1;
            dir->get_next_entry();
        }

        entry_arr->set_length(own_pr, static_cast<std::uint32_t>(entry_buf - entry_buf_org));

        LOG_TRACE("Queried entries: 0x{:x}", queried_entries);

        ctx.set_request_status(KErrNone);
    }
}