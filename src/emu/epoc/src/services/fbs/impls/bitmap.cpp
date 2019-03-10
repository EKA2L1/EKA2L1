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
#include <common/fileutils.h>
#include <common/log.h>

#include <epoc/services/fbs/fbs.h>
#include <epoc/services/fs/fs.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/vfs.h>

namespace eka2l1 {
    struct load_bitmap_arg {
        std::uint32_t bitmap_id;
        std::int32_t share;
        std::int32_t file_offset;
    };

    struct bmp_handles {
        std::int32_t handle;
        std::int32_t server_handle;
        std::int32_t address_offset;
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

        fbsbitmap *bmp = nullptr;
        fbs_server *fbss = server<fbs_server>();

        fbsbitmap_cache_info cache_info_;

        // Check if it's shared first
        if (load_options->share) {
            // Shared bitmaps are stored on server's map, because it's means to be accessed by
            // other fbs clients. Let's lookup our bitmap on there
            cache_info_.bitmap_idx = load_options->bitmap_id;
            cache_info_.last_access_time_since_ad = source->last_modify_since_1ad();
            cache_info_.path = source->file_name();

            auto shared_bitmap_ite = fbss->shared_bitmaps.find(cache_info_);

            if (shared_bitmap_ite != fbss->shared_bitmaps.end()) {
                bmp = shared_bitmap_ite->second;
            }
        }

        if (!bmp) {
            // Let's load the MBM from file first
            eka2l1::ro_file_stream stream_(source);
            loader::mbm_file mbmf_(reinterpret_cast<common::ro_stream*>(&stream_));

            mbmf_.do_read_headers();

            // Let's do an insanity check. Is the bitmap index client given us is not valid ?
            // What i mean, maybe it's out of range. There may be only 5 bitmaps, but client gives us index 6.
            if (mbmf_.trailer.count < load_options->bitmap_id) {
                ctx->set_request_status(KErrNotFound);
                return;
            }

            // With doing that, we can now finally start loading to bitmap properly. So let's do it,
            // hesistate is bad.
            epoc::bitwise_bitmap *bws_bmp = fbss->allocate_general_data<epoc::bitwise_bitmap>();
            bws_bmp->header_ = mbmf_.sbm_headers[load_options->bitmap_id - 1];
            
            bmp = make_new<fbsbitmap>(bws_bmp, static_cast<bool>(load_options->share));
        }

        if (load_options->share) {
            fbss->shared_bitmaps.emplace(cache_info_, bmp);
        }

        // Now writes the bitmap info in
        bmp_handles handle_info;

        // Add this object to the object table!
        handle_info.handle = obj_table_.add(bmp);
        handle_info.server_handle = bmp->id;
        handle_info.address_offset = fbss->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        ctx->write_arg_pkg<bmp_handles>(0, handle_info);
        ctx->set_request_status(KErrNone);
    }
}
