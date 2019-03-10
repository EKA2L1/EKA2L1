/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <common/cvt.h>
#include <common/log.h>

#include <epoc/services/fbs/fbs.h>
#include <epoc/services/fs/fs.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/vfs.h>

namespace eka2l1 {
    struct load_bitmap_arg {
        int bitmap_id;
        int share;
        int file_offset;
    };

    void fbscli::load_bitmap(service::ipc_context *ctx) {
        // Get the FS session
        session_ptr fs_target_session = ctx->sys->get_kernel_system()->get<service::session>(*(ctx->get_arg<int>(2)));
        const std::uint32_t fs_file_handle = static_cast<std::uint32_t>(*(ctx->get_arg<int>(3)));

        auto fs_server = std::reinterpret_pointer_cast<eka2l1::fs_server>(server<fbs_server>()->fs_server);
        symfile source_file = fs_server->get_fs_node_as<eka2l1::file>(fs_target_session->unique_id(), fs_file_handle);

        if (!source_file) {
            ctx->set_request_status(KErrArgument);
            return;
        }

        load_bitmap_impl(ctx, source_file);
    }

    void fbscli::load_bitmap_impl(service::ipc_context *ctx, symfile source) {
        std::optional<load_bitmap_arg> load_options = ctx->get_arg_packed<load_bitmap_arg>(0);
        if (!load_options) {
            ctx->set_request_status(KErrArgument);
            return;
        }

        LOG_TRACE("Loading bitmap from: {}", common::ucs2_to_utf8(source->file_name()));

        // Check if it's shared first

        // Check for cache, now!
        int a = 5;
    }
}
