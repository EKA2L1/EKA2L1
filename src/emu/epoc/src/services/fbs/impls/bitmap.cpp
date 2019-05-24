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

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>

#include <epoc/services/fbs/fbs.h>
#include <epoc/services/fs/fs.h>
#include <epoc/services/window/common.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/vfs.h>

#include <common/e32inc.h>
#include <e32err.h>

#include <cassert>

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

    static int get_byte_width(const std::uint32_t pixels_width, const std::uint8_t bits_per_pixel) {
        int word_width = 0;

        switch (bits_per_pixel) {
        case 1: 
        {
            word_width = (pixels_width + 31) / 32;
            break;
        }

        case 2: 
        {
            word_width = (pixels_width + 15) / 16;
            break;
        }

        case 4: 
        {
            word_width = (pixels_width + 7) / 8;
            break;
        }

        case 8:
        {
            word_width = (pixels_width + 3) / 4;
            break;
        }

        case 12:
        case 16: 
        {
            word_width = (pixels_width + 1) / 2;
            break;
        }

        case 24:
        {
            word_width = (((pixels_width * 3) + 11) / 12) * 3;
            break;
        }

        case 32: 
        {
            word_width = pixels_width;
            break;
        }

        default: {
            assert(false);
            break;
        }
        }

        return word_width * 4;
    }

    fbsbitmap::~fbsbitmap() {
        serv_->free_bitmap(this);
    }

    std::optional<std::size_t> fbs_server::load_uncomp_data_to_rom(loader::mbm_file &mbmf_, const std::size_t idx_
        , int *err_code) {
        // First, get the size of data when uncompressed
        std::size_t size_when_uncompressed = 0;
        if (!mbmf_.read_single_bitmap(idx_, nullptr, size_when_uncompressed)) {
            *err_code = fbs_load_data_err_read_decomp_fail;
            return std::nullopt;
        }

        // Allocates from the large chunk
        // Align them with 4 bytes
        std::size_t avail_dest_size = common::align(size_when_uncompressed, 4);
        void *data = large_chunk_allocator->allocate(avail_dest_size);
        if (data == nullptr) {
            // We can't allocate at all
            *err_code = fbs_load_data_err_out_of_mem;
            return std::nullopt;
        }

        // Yay, we manage to alloc memory to load the data in
        // So let's get to work
        if (!mbmf_.read_single_bitmap(idx_, reinterpret_cast<std::uint8_t*>(data), avail_dest_size)) {
            *err_code = fbs_load_data_err_read_decomp_fail;
            return std::nullopt;
        }

        *err_code = fbs_load_data_err_none;
        return reinterpret_cast<std::uint8_t*>(data) - base_large_chunk;
    }

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

    void fbscli::duplicate_bitmap(service::ipc_context *ctx) {
        fbsbitmap *bmp = server<fbs_server>()->get<fbsbitmap>(static_cast<std::uint32_t>(*(ctx->get_arg<int>(0))));
        if (!bmp) {
            ctx->set_request_status(KErrBadHandle);
            return;
        }

        bmp_handles handle_info;
        
        // Add this object to the object table!
        handle_info.handle = obj_table_.add(bmp);
        handle_info.server_handle = bmp->id;
        handle_info.address_offset = server<fbs_server>()->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        ctx->write_arg_pkg(1, handle_info);
        ctx->set_request_status(KErrNone);
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

            // Load the bitmap data to large chunk
            int err_code = fbs_load_data_err_none;
            auto bmp_data_offset = fbss->load_uncomp_data_to_rom(mbmf_, load_options->bitmap_id - 1, &err_code);

            if (!bmp_data_offset) {
                switch (err_code) {
                case fbs_load_data_err_out_of_mem: {
                    LOG_ERROR("Can't allocate data for storing bitmap!");
                    ctx->set_request_status(KErrNoMemory);

                    return;
                }

                case fbs_load_data_err_read_decomp_fail: {
                    LOG_ERROR("Can't read or decompress bitmap data, possibly corrupted.");
                    ctx->set_request_status(KErrCorrupt);

                    return;
                }

                default: {
                    LOG_ERROR("Unknown error code from loading uncompressed bitmap!");
                    ctx->set_request_status(KErrGeneral);

                    return;
                }
                }
            }

            // Place holder value indicates we allocate through the large chunk.
            // If guest uses this, than we are doom. But gonna put it here anyway
            bws_bmp->pile_ = 0x1EA5EB0;
            bws_bmp->data_offset_ = static_cast<std::uint32_t>(bmp_data_offset.value());
            bws_bmp->compressed_in_ram_ = false;
            bws_bmp->byte_width_ = get_byte_width(bws_bmp->header_.size_pixels.x, bws_bmp->header_.bit_per_pixels);
            bws_bmp->uid_ = epoc::bitwise_bitmap_uid;

            bmp = make_new<fbsbitmap>(fbss, bws_bmp, static_cast<bool>(load_options->share));
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

    struct bmp_specs {
        eka2l1::vec2 size;
        epoc::display_mode bpp;       // Ignore
        std::uint32_t handle;
        std::uint32_t server_handle;
        std::uint32_t address_offset;
    };

    static int get_bpp_from_display_mode(const epoc::display_mode bpp) {
        switch (bpp) {
        case epoc::display_mode::gray2: return 1;
        case epoc::display_mode::gray4: return 2;
        case epoc::display_mode::gray16: case epoc::display_mode::color16: return 4;
        case epoc::display_mode::gray256: case epoc::display_mode::color256: return 8;
        case epoc::display_mode::color4k: return 12;
        case epoc::display_mode::color64k: return 16;
        case epoc::display_mode::color16m: return 24;
        case epoc::display_mode::color16mu: case epoc::display_mode::color16ma: return 32;
        }

        return 24;
    }

    static std::size_t calculate_aligned_bitmap_bytes(const eka2l1::vec2 &size, const epoc::display_mode bpp) {
        if (size.x == 0 || size.y == 0) {
            return 0;
        }

        return get_byte_width(size.x, get_bpp_from_display_mode(bpp)) * size.y;
    }

    fbsbitmap *fbs_server::create_bitmap(const eka2l1::vec2 &size, const epoc::display_mode dpm) {
        epoc::bitwise_bitmap *bws_bmp = allocate_general_data<epoc::bitwise_bitmap>();
        bws_bmp->header_.size_pixels = size;
        bws_bmp->header_.bit_per_pixels = get_bpp_from_display_mode(dpm);
        bws_bmp->data_offset_ = 0;

        // Calculate the size
        std::size_t alloc_bytes = calculate_aligned_bitmap_bytes(size, dpm);
        
        if (alloc_bytes > 0) {
            // Allocates from the large chunk
            // Align them with 4 bytes
            std::size_t avail_dest_size = common::align(alloc_bytes, 4);
            void *data = large_chunk_allocator->allocate(avail_dest_size);

            if (!data) {
                return nullptr;
            }

            bws_bmp->data_offset_ = static_cast<int>(reinterpret_cast<std::uint8_t*>(data) - base_large_chunk);
        }

        fbsbitmap *bmp = make_new<fbsbitmap>(this, bws_bmp, false);
        return bmp;
    }

    bool fbs_server::free_bitmap(fbsbitmap *bmp) {
        if (!bmp->bitmap_ || bmp->count > 0) {
            return false;
        }

        // First, free the bitmap pixels.
        if (!large_chunk_allocator->free(base_large_chunk + bmp->bitmap_->data_offset_)) {
            return false;
        }

        // Free the bitwise bitmap.
        if (!free_general_data(bmp->bitmap_)) {
            return false;
        }

        return true;
    }
    
    void fbscli::create_bitmap(service::ipc_context *ctx) {
        std::optional<bmp_specs> specs = ctx->get_arg_packed<bmp_specs>(0);

        if (!specs) {
            ctx->set_request_status(KErrArgument);
            return;
        }

        fbs_server *fbss = server<fbs_server>();
        fbsbitmap *bmp = fbss->create_bitmap(specs->size, specs->bpp);

        if (!bmp) {
            ctx->set_request_status(KErrNoMemory);
            return;
        }

        specs->handle = obj_table_.add(bmp);
        specs->server_handle = bmp->id;
        specs->address_offset = fbss->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        ctx->write_arg_pkg(0, specs.value());
        ctx->set_request_status(KErrNone);
    }
}
