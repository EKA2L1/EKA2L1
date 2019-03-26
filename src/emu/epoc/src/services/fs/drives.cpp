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

#include <epoc/vfs.h>
#include <epoc/epoc.h>

#include <common/e32inc.h>
#include <e32err.h>

#include <cwctype>

namespace eka2l1 {
    void fill_drive_info(epoc::fs::drive_info *info, eka2l1::drive &io_drive);

    void fs_server_client::file_drive(service::ipc_context ctx) {
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

        drive_number drv = static_cast<drive_number>(std::towlower(f->file_name()[0]) - 'a');
        epoc::fs::drive_info info;

        std::optional<eka2l1::drive> io_drive = ctx.sys->get_io_system()->get_drive_entry(
            static_cast<drive_number>(drv));

        if (!io_drive) {
            info.type = epoc::fs::media_unknown;
        } else {
            fill_drive_info(&(info), *io_drive);
        }

        ctx.write_arg_pkg<drive_number>(0, drv);
        ctx.write_arg_pkg<epoc::fs::drive_info>(1, info);

        ctx.set_request_status(KErrNone);
    }

    void fill_drive_info(epoc::fs::drive_info *info, eka2l1::drive &io_drive) {
        info->drive_att = 0;
        info->media_att = 0;

        if (io_drive.media_type == drive_media::none) {
            info->type= epoc::fs::media_unknown;
            return;
        }

        switch (io_drive.media_type) {
        case drive_media::physical: {
            info->type = epoc::fs::media_hard_disk;
            info->drive_att = epoc::fs::drive_att_local;

            break;
        }

        case drive_media::rom: {
            info->type = epoc::fs::media_rom;
            info->drive_att = epoc::fs::drive_att_rom;

            break;
        }

        case drive_media::reflect: {
            info->type = epoc::fs::media_rotating;
            info->drive_att = epoc::fs::drive_att_redirected;

            break;
        }

        default:
            break;
        }

        info->connection_bus_type = epoc::fs::connection_bus_internal;
        info->battery = epoc::fs::battery_state_not_supported;

        if (static_cast<int>(io_drive.attribute & io_attrib::hidden)) {
            info->drive_att |= epoc::fs::drive_att_hidden;
        }

        if (static_cast<int>(io_drive.attribute & io_attrib::internal)) {
            info->drive_att |= epoc::fs::drive_att_internal;
        }

        if (static_cast<int>(io_drive.attribute & io_attrib::removeable)) {
            info->drive_att |= epoc::fs::drive_att_logically_removeable;
        }

        if (static_cast<int>(io_drive.attribute & io_attrib::write_protected)) {
            info->drive_att |= epoc::fs::media_att_write_protected;
        }
    }

    /* Simple for now only, in the future this should be more advance. */
    void fs_server::drive(service::ipc_context ctx) {
        drive_number drv = static_cast<drive_number>(*ctx.get_arg<int>(1));
        std::optional<epoc::fs::drive_info> info = ctx.get_arg_packed<epoc::fs::drive_info>(0);

        if (!info) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::optional<eka2l1::drive> io_drive = 
            ctx.sys->get_io_system()->get_drive_entry(static_cast<drive_number>(drv));

        if (!io_drive) {
            info->type = epoc::fs::media_unknown;
        } else {
            fill_drive_info(&(*info), *io_drive);
        }

        ctx.write_arg_pkg<epoc::fs::drive_info>(0, *info);
        ctx.set_request_status(KErrNone);
    }

    void fs_server::drive_list(service::ipc_context ctx) {
        std::optional<int> flags = ctx.get_arg<int>(1);

        if (!flags) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        std::vector<io_attrib> exclude_attribs;
        std::vector<io_attrib> include_attribs;

        // Fetch flags
        if (*flags & epoc::fs::drive_att_hidden) {
            if (*flags & epoc::fs::drive_att_exclude) {
                exclude_attribs.push_back(io_attrib::hidden);
            } else {
                include_attribs.push_back(io_attrib::hidden);
            }
        }

        std::array<char, drive_count> dlist;

        std::fill(dlist.begin(), dlist.end(), 0);

        for (size_t i = drive_a; i < drive_count; i += 1) {
            auto drv_op = ctx.sys->get_io_system()->get_drive_entry(
                static_cast<drive_number>(i));

            if (drv_op) {
                eka2l1::drive drv = std::move(*drv_op);

                bool out = false;

                for (const auto &exclude : exclude_attribs) {
                    if (static_cast<int>(exclude) & static_cast<int>(drv.attribute)) {
                        dlist[i] = 0;
                        out = true;

                        break;
                    }
                }

                if (!out) {
                    if (include_attribs.empty()) {
                        if (drv.media_type != drive_media::none) {
                            dlist[i] = 1;
                        }

                        continue;
                    }

                    auto meet_one_condition = std::find_if(include_attribs.begin(), include_attribs.end(),
                        [=](io_attrib attrib) { return static_cast<int>(attrib) & static_cast<int>(drv.attribute); });

                    if (meet_one_condition != include_attribs.end()) {
                        dlist[i] = 1;
                    }
                }
            }
        }

        bool success = ctx.write_arg_pkg(0, reinterpret_cast<uint8_t *>(&dlist[0]),
            static_cast<std::uint32_t>(dlist.size()));

        if (!success) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        ctx.set_request_status(KErrNone);
    }

    void fs_server::volume(service::ipc_context ctx) {
        std::optional<epoc::fs::volume_info> info = ctx.get_arg_packed<epoc::fs::volume_info>(0);

        if (!info) {
            ctx.set_request_status(KErrArgument);
            return;
        }

        drive_number drv = static_cast<drive_number>(*ctx.get_arg<int>(1));
        std::optional<eka2l1::drive> io_drive = ctx.sys->get_io_system()->get_drive_entry(static_cast<drive_number>(drv));

        if (!io_drive) {
            info->drv_info.type = epoc::fs::media_unknown;
        } else {
            fill_drive_info(&info->drv_info, *io_drive);
        }

        info->uid = drv;

        LOG_WARN("Volume size stubbed with 1GB");

        // Stub this
        info->size = common::GB(1);
        info->free = common::GB(1);

        ctx.write_arg_pkg<epoc::fs::volume_info>(0, *info);
        ctx.set_request_status(KErrNone);
    }

    void fs_server::query_drive_info_ext(service::ipc_context ctx) {
        drive_number drv = static_cast<drive_number>(*ctx.get_arg<int>(0));
        std::optional<eka2l1::drive> io_drive = ctx.sys->get_io_system()->get_drive_entry(drv);

        // If the drive hasn't been mounted yet, return KErrNotFound
        if (!io_drive) {
            ctx.set_request_status(KErrNotFound);
            return;
        }

        epoc::fs::extended_fs_query_command query_cmd = 
            static_cast<decltype(query_cmd)>(*ctx.get_arg<int>(1));

        switch (query_cmd) {
        case epoc::fs::extended_fs_query_command::file_system_sub_type: {
            // Query file system type. Using FAT32 as default.
            ctx.write_arg(2, u"FAT32");
            break;
        }

        case epoc::fs::extended_fs_query_command::is_drive_sync: {
            // Check if drive is sync. Yes in this case.
            bool result = true;

            ctx.write_arg_pkg(2, result);
            break;
        }

        case epoc::fs::extended_fs_query_command::is_drive_finalised: {
            bool result = true;

            // Check if drive is safe to remove. Yes ?
            LOG_WARN("Checking if drive is finalised, stubbed");
            ctx.write_arg_pkg(2, result);
            break;
        }

        case epoc::fs::extended_fs_query_command::io_param_info: {
            epoc::fs::io_drive_param_info param;
            param.block_size = 512;
            param.cluster_size = 4096;
            param.max_supported_file_size = 0xFFFFFFFF;
            param.rec_read_buf_size = 8192;
            param.rec_write_buf_size = 16384;

            LOG_INFO("IOParamInfo stubbed");
            ctx.write_arg_pkg(2, param);

            break;
        }

        default: {
            LOG_ERROR("Unimplemented extended query drive opcode: 0x{:x}", static_cast<int>(query_cmd));
            break;
        }
        }

        ctx.set_request_status(KErrNone);
    }
}
