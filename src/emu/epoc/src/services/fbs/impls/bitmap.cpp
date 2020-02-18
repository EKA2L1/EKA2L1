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
#include <common/bitmap.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>

#include <epoc/services/fbs/fbs.h>
#include <epoc/services/fbs/palette.h>
#include <epoc/services/fs/fs.h>
#include <epoc/services/window/common.h>

#include <epoc/epoc.h>
#include <epoc/kernel.h>
#include <epoc/vfs.h>
#include <epoc/utils/err.h>

#include <cassert>
#include <algorithm>

namespace eka2l1 {
    static epoc::display_mode get_display_mode_from_bpp(const int bpp) {
        switch (bpp) {
        case 1:
            return epoc::display_mode::gray2;
        case 2:
            return epoc::display_mode::gray4;
        case 4:
            return epoc::display_mode::color16;
        case 8:
            return epoc::display_mode::color256;
        case 12:
            return epoc::display_mode::color4k;
        case 16:
            return epoc::display_mode::color64k;
        case 24:
            return epoc::display_mode::color16m;
        case 32:
            return epoc::display_mode::color16ma;
        default:
            break;
        }

        return epoc::display_mode::color16m;
    }

    static int get_bpp_from_display_mode(const epoc::display_mode bpp) {
        switch (bpp) {
        case epoc::display_mode::gray2:
            return 1;
        case epoc::display_mode::gray4:
            return 2;
        case epoc::display_mode::gray16:
        case epoc::display_mode::color16:
            return 4;
        case epoc::display_mode::gray256:
        case epoc::display_mode::color256:
            return 8;
        case epoc::display_mode::color4k:
            return 12;
        case epoc::display_mode::color64k:
            return 16;
        case epoc::display_mode::color16m:
            return 24;
        case epoc::display_mode::color16mu:
        case epoc::display_mode::color16ma:
            return 32;
        }

        return 24;
    }

    static epoc::bitmap_color get_bitmap_color_type_from_display_mode(const epoc::display_mode bpp) {
        switch (bpp) {
        case epoc::display_mode::gray2:
            return epoc::monochrome_bitmap;
        case epoc::display_mode::color16ma:
        case epoc::display_mode::color16map:
            return epoc::color_bitmap_with_alpha;
        default:
            break;
        }

        return epoc::color_bitmap;
    }

    static int get_byte_width(const std::uint32_t pixels_width, const std::uint8_t bits_per_pixel) {
        int word_width = 0;

        switch (bits_per_pixel) {
        case 1: {
            word_width = (pixels_width + 31) / 32;
            break;
        }

        case 2: {
            word_width = (pixels_width + 15) / 16;
            break;
        }

        case 4: {
            word_width = (pixels_width + 7) / 8;
            break;
        }

        case 8: {
            word_width = (pixels_width + 3) / 4;
            break;
        }

        case 12:
        case 16: {
            word_width = (pixels_width + 1) / 2;
            break;
        }

        case 24: {
            word_width = (((pixels_width * 3) + 11) / 12) * 3;
            break;
        }

        case 32: {
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

    namespace epoc {
        constexpr std::uint32_t BITWISE_BITMAP_UID = 0x10000040;
        constexpr std::uint32_t MAGIC_FBS_PILE_PTR = 0xDEADFB55;

        // Magic heap pointer so the client knows that we allocate from the large chunk.
        // The 8 is actually H, so you get my idea.
        constexpr std::uint32_t MAGIC_FBS_HEAP_PTR = 0xDEADFB88;

        display_mode bitwise_bitmap::settings::initial_display_mode() const {
            return static_cast<display_mode>(flags_ & 0x000000FF);
        }

        display_mode bitwise_bitmap::settings::current_display_mode() const {
            return static_cast<display_mode>((flags_ & 0x0000FF00) >> 8);
        }

        void bitwise_bitmap::settings::current_display_mode(const display_mode &mode) {
            flags_ &= 0xFFFF00FF;
            flags_ |= (static_cast<std::uint32_t>(mode) << 8);
        }

        void bitwise_bitmap::settings::initial_display_mode(const display_mode &mode) {
            flags_ &= 0xFFFFFF00;
            flags_ |= static_cast<std::uint32_t>(mode);
        }

        bool bitwise_bitmap::settings::dirty_bitmap() const {
            return flags_ & settings_flag::dirty_bitmap;
        }

        void bitwise_bitmap::settings::dirty_bitmap(const bool is_it) {
            if (is_it)
                flags_ |= settings_flag::dirty_bitmap;
            else
                flags_ &= ~(settings_flag::dirty_bitmap);
        }

        bool bitwise_bitmap::settings::violate_bitmap() const {
            return flags_ & settings_flag::violate_bitmap;
        }

        void bitwise_bitmap::settings::violate_bitmap(const bool is_it) {
            if (is_it)
                flags_ |= settings_flag::violate_bitmap;
            else
                flags_ &= ~(settings_flag::violate_bitmap);
        }

        static void do_white_fill(std::uint8_t *dest, const std::size_t size, epoc::display_mode mode) {
            std::fill(dest, dest + size, 0xFF);
        }
        
        void bitwise_bitmap::construct(loader::sbm_header &info, epoc::display_mode disp_mode, void *data, const void *base, const bool white_fill) {
            uid_ = epoc::BITWISE_BITMAP_UID;
            allocator_ = MAGIC_FBS_HEAP_PTR;
            pile_ = MAGIC_FBS_PILE_PTR;
            byte_width_ = 0;
            header_ = info;
            spare1_ = 0;
            data_offset_ = 0xFFFFFF;

            if (info.compression != epoc::bitmap_file_no_compression) {
                // Compressed in RAM!
                compressed_in_ram_ = true;
            }

            if (data) {
                data_offset_ = static_cast<int>(reinterpret_cast<const std::uint8_t *>(data) - reinterpret_cast<const std::uint8_t *>(base));
            } else {
                data_offset_ = 0;
            }

            settings_.current_display_mode(disp_mode);
            settings_.initial_display_mode(disp_mode);

            byte_width_ = get_byte_width(info.size_pixels.width(), static_cast<std::uint8_t>(info.bit_per_pixels));

            if (white_fill && (data_offset_ != 0)) {
                do_white_fill(reinterpret_cast<std::uint8_t *>(data), info.bitmap_size - sizeof(loader::sbm_header), settings_.current_display_mode());
            }
        }

        int bitwise_bitmap::copy_data(const bitwise_bitmap& source, uint8_t *base) {
            const auto disp_mode = source.settings_.current_display_mode();
            assert(disp_mode == settings_.current_display_mode());
            if (source.header_.size_pixels.width() > 0) {
                header_.size_twips.x = source.header_.size_twips.width() * header_.size_pixels.width() / source.header_.size_pixels.width();
            }
            if (source.header_.size_pixels.height() > 0) {
                header_.size_twips.y = source.header_.size_twips.height() * header_.size_pixels.height() / source.header_.size_pixels.height();
            }

            uint8_t *dest_base = base + data_offset_;
            uint8_t *src_base = base + source.data_offset_;
            const int min_pixel_height = std::min(header_.size_pixels.height(), source.header_.size_pixels.height());

            // copy with compressed data not supported yet
            assert(source.compressed_in_ram_ == false);

            uint8_t *dest = dest_base;
            uint8_t *src = src_base;
            const int min_byte_width = std::min(byte_width_, source.byte_width_);
            for (int row = 0; row < min_pixel_height; row++) {
                std::memcpy(dest, src, min_byte_width);
                dest += byte_width_;
                src += source.byte_width_;
            }

            if (header_.size_pixels.width() > source.header_.size_pixels.width()) {
                const int extra_bits = (source.header_.size_pixels.width() * source.header_.bit_per_pixels) & 31;
                if (extra_bits > 0) {
                    uint32_t mask = 0xFFFFFFFF;
                    mask <<= extra_bits;
                    const int dest_word_width = byte_width_ >> 2;
                    const int src_word_width = source.byte_width_ >> 2;
                    uint32_t *mask_addr = reinterpret_cast<uint32_t*>(dest_base) + src_word_width - 1;
                    for (int row = 0; row < min_pixel_height; row ++) {
                        *mask_addr |= mask;
                        mask_addr += dest_word_width;
                    }
                }
            }
            return epoc::error_none;
        }
    }

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

    fbsbitmap::~fbsbitmap() {
        serv_->free_bitmap(this);
    }

    std::optional<std::size_t> fbs_server::load_data_to_rom(loader::mbm_file &mbmf_, const std::size_t idx_, int *err_code) {
        // First, get the size of data when compressed
        std::size_t size_when_compressed = 0;
        if (!mbmf_.read_single_bitmap_raw(idx_, nullptr, size_when_compressed)) {
            *err_code = fbs_load_data_err_read_decomp_fail;
            return std::nullopt;
        }

        // Allocates from the large chunk
        // Align them with 4 bytes
        std::size_t avail_dest_size = common::align(size_when_compressed, 4);
        void *data = large_chunk_allocator->allocate(avail_dest_size);
        if (data == nullptr) {
            // We can't allocate at all
            *err_code = fbs_load_data_err_out_of_mem;
            return std::nullopt;
        }

        // Yay, we manage to alloc memory to load the data in
        // So let's get to work
        if (!mbmf_.read_single_bitmap_raw(idx_, reinterpret_cast<std::uint8_t *>(data), avail_dest_size)) {
            *err_code = fbs_load_data_err_read_decomp_fail;
            return std::nullopt;
        }

        *err_code = fbs_load_data_err_none;
        return reinterpret_cast<std::uint8_t *>(data) - base_large_chunk;
    }

    void fbscli::load_bitmap(service::ipc_context *ctx) {
        // Get the FS session
        session_ptr fs_target_session = ctx->sys->get_kernel_system()->get<service::session>(*(ctx->get_arg<std::int32_t>(2)));
        const std::uint32_t fs_file_handle = *(ctx->get_arg<std::uint32_t>(3));

        auto fs_server = reinterpret_cast<eka2l1::fs_server*>(server<fbs_server>()->fs_server);
        file *source_file = fs_server->get_file(fs_target_session->unique_id(), fs_file_handle);

        if (!source_file) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        load_bitmap_impl(ctx, source_file);
    }

    void fbscli::load_bitmap_fast(service::ipc_context *ctx) {
        std::optional<std::u16string> name = ctx->get_arg<std::u16string>(2);
        if (!name.has_value()) {
            ctx->set_request_status(epoc::error_not_found);
            return;
        }

        symfile source_file = ctx->sys->get_io_system()->open_file(name.value(), READ_MODE | BIN_MODE);

        if (!source_file) {
            ctx->set_request_status(epoc::error_not_found);
            return;
        }

        load_bitmap_impl(ctx, source_file.get());
    }

    void fbscli::duplicate_bitmap(service::ipc_context *ctx) {
        fbsbitmap *bmp = server<fbs_server>()->get<fbsbitmap>(*(ctx->get_arg<std::uint32_t>(0)));
        if (!bmp) {
            ctx->set_request_status(epoc::error_bad_handle);
            return;
        }

        bmp_handles handle_info;

        // Add this object to the object table!
        handle_info.handle = obj_table_.add(bmp);
        handle_info.server_handle = bmp->id;
        handle_info.address_offset = server<fbs_server>()->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        ctx->write_arg_pkg(1, handle_info);
        ctx->set_request_status(epoc::error_none);
    }

    void fbscli::load_bitmap_impl(service::ipc_context *ctx, file *source) {
        std::optional<load_bitmap_arg> load_options = ctx->get_arg_packed<load_bitmap_arg>(1);
        if (!load_options) {
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        LOG_TRACE("Loading bitmap from: {}, id: {}", common::ucs2_to_utf8(source->file_name()),
            load_options->bitmap_id);

        fbsbitmap *bmp = nullptr;
        fbs_server *fbss = server<fbs_server>();

        fbsbitmap_cache_info cache_info_;
        bool already_cache = false;

        // Check if it's shared first
        if (load_options->share) {
            // Shared bitmaps are stored on server's map, because it's means to be accessed by
            // other fbs clients. Let's lookup our bitmap on there
            cache_info_.bitmap_idx = load_options->bitmap_id;
            cache_info_.path = source->file_name();

            auto shared_bitmap_ite = fbss->shared_bitmaps.find(cache_info_);

            if (shared_bitmap_ite != fbss->shared_bitmaps.end()) {
                bmp = shared_bitmap_ite->second;
                already_cache = true;
            }
        }

        if (!bmp) {
            // Let's load the MBM from file first
            eka2l1::ro_file_stream stream_(source);
            loader::mbm_file mbmf_(reinterpret_cast<common::ro_stream *>(&stream_));

            if (!mbmf_.do_read_headers()) {
                ctx->set_request_status(epoc::error_corrupt);
                return;
            }

            // Let's do an insanity check. Is the bitmap index client given us is not valid ?
            // What i mean, maybe it's out of range. There may be only 5 bitmaps, but client gives us index 5.
            if (mbmf_.trailer.count <= load_options->bitmap_id) {
                ctx->set_request_status(epoc::error_not_found);
                return;
            }

            // With doing that, we can now finally start loading to bitmap properly. So let's do it,
            // hesistate is bad.
            epoc::bitwise_bitmap *bws_bmp = fbss->allocate_general_data<epoc::bitwise_bitmap>();
            bws_bmp->header_ = mbmf_.sbm_headers[load_options->bitmap_id];

            // Load the bitmap data to large chunk
            int err_code = fbs_load_data_err_none;
            auto bmp_data_offset = fbss->load_data_to_rom(mbmf_, load_options->bitmap_id , &err_code);

            if (!bmp_data_offset) {
                switch (err_code) {
                case fbs_load_data_err_out_of_mem: {
                    LOG_ERROR("Can't allocate data for storing bitmap!");
                    ctx->set_request_status(epoc::error_no_memory);

                    return;
                }

                case fbs_load_data_err_read_decomp_fail: {
                    LOG_ERROR("Can't read or decompress bitmap data, possibly corrupted.");
                    ctx->set_request_status(epoc::error_corrupt);

                    return;
                }

                default: {
                    LOG_ERROR("Unknown error code from loading uncompressed bitmap!");
                    ctx->set_request_status(epoc::error_general);

                    return;
                }
                }
            }

            // Place holder value indicates we allocate through the large chunk.
            // If guest uses this, than we are doom. But gonna put it here anyway
            bws_bmp->pile_ = epoc::MAGIC_FBS_PILE_PTR;
            bws_bmp->allocator_ = epoc::MAGIC_FBS_HEAP_PTR;

            bws_bmp->data_offset_ = static_cast<std::uint32_t>(bmp_data_offset.value());
            bws_bmp->compressed_in_ram_ = false;
            bws_bmp->byte_width_ = get_byte_width(bws_bmp->header_.size_pixels.x, bws_bmp->header_.bit_per_pixels);
            bws_bmp->uid_ = epoc::bitwise_bitmap_uid;

            // Get display mode
            const epoc::display_mode dpm = get_display_mode_from_bpp(bws_bmp->header_.bit_per_pixels);
            bws_bmp->settings_.initial_display_mode(dpm);
            bws_bmp->settings_.current_display_mode(dpm);

            bmp = make_new<fbsbitmap>(fbss, bws_bmp, static_cast<bool>(load_options->share), support_dirty_bitmap);
        }

        if (load_options->share && !already_cache) {
            fbss->shared_bitmaps.emplace(cache_info_, bmp);
            bmp->ref();
        }

        // Now writes the bitmap info in
        bmp_handles handle_info;

        // Add this object to the object table!
        handle_info.handle = obj_table_.add(bmp);
        handle_info.server_handle = bmp->id;
        handle_info.address_offset = fbss->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        ctx->write_arg_pkg<bmp_handles>(0, handle_info);
        ctx->set_request_status(epoc::error_none);
    }

    struct bmp_specs {
        eka2l1::vec2 size;
        epoc::display_mode bpp; // Ignore
        std::uint32_t handle;
        std::uint32_t server_handle;
        std::uint32_t address_offset;
    };

    static std::size_t calculate_aligned_bitmap_bytes(const eka2l1::vec2 &size, const epoc::display_mode bpp) {
        if (size.x == 0 || size.y == 0) {
            return 0;
        }

        return get_byte_width(size.x, get_bpp_from_display_mode(bpp)) * size.y;
    }

    fbsbitmap *fbs_server::create_bitmap(const eka2l1::vec2 &size, const epoc::display_mode dpm, const bool alloc_data, const bool support_dirty) {
        epoc::bitwise_bitmap *bws_bmp = allocate_general_data<epoc::bitwise_bitmap>();

        // Calculate the size
        std::size_t alloc_bytes = calculate_aligned_bitmap_bytes(size, dpm);
        void *data = nullptr;

        if ((alloc_bytes > 0) && alloc_data) {
            // Allocates from the large chunk
            // Align them with 4 bytes
            std::size_t avail_dest_size = common::align(alloc_bytes, 4);
            data = large_chunk_allocator->allocate(avail_dest_size);

            if (!data) {
                shared_chunk_allocator->free(bws_bmp);
                return nullptr;
            }
        }

        loader::sbm_header header;
        header.compression = epoc::bitmap_file_no_compression;
        header.bitmap_size = static_cast<std::uint32_t>(alloc_bytes + sizeof(loader::sbm_header));
        header.size_pixels = size;
        header.color = get_bitmap_color_type_from_display_mode(dpm);
        header.header_len = sizeof(loader::sbm_header);
        header.palette_size = 0;
        header.size_twips = size * twips_mul;
        header.bit_per_pixels = get_bpp_from_display_mode(dpm);

        bws_bmp->construct(header, dpm, data, base_large_chunk, true);

        fbsbitmap *bmp = make_new<fbsbitmap>(this, bws_bmp, false, support_dirty);

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
            ctx->set_request_status(epoc::error_argument);
            return;
        }

        fbs_server *fbss = server<fbs_server>();
        fbsbitmap *bmp = fbss->create_bitmap(specs->size, specs->bpp);

        if (!bmp) {
            ctx->set_request_status(epoc::error_no_memory);
            return;
        }

        specs->handle = obj_table_.add(bmp);
        specs->server_handle = bmp->id;
        specs->address_offset = fbss->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        ctx->write_arg_pkg(0, specs.value());
        ctx->set_request_status(epoc::error_none);
    }

    void fbscli::resize_bitmap(service::ipc_context *ctx) {
        const auto fbss = server<fbs_server>();
        const epoc::handle handle = *(ctx->get_arg<std::uint32_t>(0));
        fbsbitmap *bmp = fbss->get<fbsbitmap>(handle);
        if (!bmp) {
            ctx->set_request_status(epoc::error_bad_handle);
            return;
        }

        while (bmp->clean_bitmap != nullptr) {
            bmp = bmp->clean_bitmap;
        }

        const vec2 new_size = {*(ctx->get_arg<int>(1)), *(ctx->get_arg<int>(2))};
        const bool compressed_in_ram = bmp->bitmap_->compressed_in_ram_;

        // not working with compressed bitmaps right now
        assert(compressed_in_ram == false);

        const epoc::display_mode disp_mode = bmp->bitmap_->settings_.current_display_mode();
        const auto new_bmp = fbss->create_bitmap(new_size, disp_mode);

        new_bmp->bitmap_->copy_data(*(bmp->bitmap_), fbss->base_large_chunk);
        bmp->clean_bitmap = new_bmp;

        // notify dirty bitmap on ref count >= 2

        obj_table_.remove(handle);
        bmp_handles handle_info;

        // Add this object to the object table!
        handle_info.handle = obj_table_.add(new_bmp);
        handle_info.server_handle = new_bmp->id;
        handle_info.address_offset = server<fbs_server>()->host_ptr_to_guest_shared_offset(new_bmp->bitmap_);

        ctx->write_arg_pkg(3, handle_info);
        ctx->set_request_status(epoc::error_none);
    }

    void fbscli::notify_dirty_bitmap(service::ipc_context *ctx) {
        if (dirty_nof_.empty()) {
            dirty_nof_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);

            if (server<fbs_server>()->compressor) {
                server<fbs_server>()->compressor->notify(dirty_nof_);
            }
        }
    }

    void fbscli::cancel_notify_dirty_bitmap(service::ipc_context *ctx) {
        if (server<fbs_server>()->compressor)
            server<fbs_server>()->compressor->cancel(dirty_nof_);
        else
            dirty_nof_.complete(epoc::error_cancel);

        ctx->set_request_status(epoc::error_none);
    }

    void fbscli::get_clean_bitmap(service::ipc_context *ctx) {
        const epoc::handle bmp_handle = *ctx->get_arg<epoc::handle>(0);
        fbsbitmap *bmp = obj_table_.get<fbsbitmap>(bmp_handle);

        if (!bmp) {
            ctx->set_request_status(epoc::error_bad_handle);
            return;
        }

        while (bmp->clean_bitmap != nullptr) {
            bmp = bmp->clean_bitmap;
        }

        bmp_handles handle_info;

        // Get the clean bitmap handle!
        handle_info.handle = obj_table_.add(bmp);
        handle_info.server_handle = bmp->id;
        handle_info.address_offset = server<fbs_server>()->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        // Close the old handle. To prevent this object from being destroyed.
        // In case no clean bitmap at all!
        obj_table_.remove(bmp_handle);

        ctx->write_arg_pkg(1, handle_info);
        ctx->set_request_status(epoc::error_none);
    }

    namespace epoc {
        bool save_bwbmp_to_file(const std::string &destination, epoc::bitwise_bitmap *bitmap, const char *base) {
            if (bitmap->header_.compression != epoc::bitmap_file_no_compression) {
                return false;
            }
            
            bool need_process = false;

            common::dib_header_v2 dib_header;

            dib_header.uncompressed_size = static_cast<std::uint32_t>(calculate_aligned_bitmap_bytes
                (bitmap->header_.size_pixels, bitmap->settings_.current_display_mode()));
            dib_header.bit_per_pixels = bitmap->header_.bit_per_pixels;
            dib_header.color_plane_count = 1;
            dib_header.important_color_count = 0;
            dib_header.palette_count = 0;
            dib_header.size = bitmap->header_.size_pixels;

            switch (dib_header.bit_per_pixels) {
            case 8: case 16: {
                dib_header.header_size = sizeof(common::dib_header_v1);
                dib_header.comp = 0;
                dib_header.bit_per_pixels = 24;
                need_process = true;

                break;
            }

            default: {
                dib_header.header_size = sizeof(common::dib_header_v1);
                dib_header.comp = 0;

                break;
            }
            }

            dib_header.size.y = -dib_header.size.y;
            dib_header.print_res = bitmap->header_.size_twips;

            common::bmp_header header;
            header.file_size = static_cast<std::uint32_t>(sizeof(common::bmp_header) + dib_header.header_size +
                dib_header.uncompressed_size);
            header.pixel_array_offset = static_cast<std::uint32_t>(sizeof(common::bmp_header) + dib_header.header_size);

            std::ofstream file(destination);

            if (file.fail()) {
                return false;
            }

            file.write(reinterpret_cast<const char*>(&header), sizeof(common::bmp_header));
            file.write(reinterpret_cast<const char*>(&dib_header), dib_header.header_size);

            if (need_process) {
                const std::uint32_t byte_width = get_byte_width(bitmap->header_.size_pixels.x, 
                    bitmap->header_.bit_per_pixels);

                const std::uint8_t *packed_data = reinterpret_cast<const std::uint8_t*>(bitmap->data_offset_ + base);

                switch (bitmap->settings_.current_display_mode()) {
                case epoc::display_mode::color64k:
                    for (std::size_t y = 0; y < bitmap->header_.size_pixels.y; y++) {
                        for (std::size_t x = 0; x < bitmap->header_.size_pixels.x; x++) {
                            const std::uint16_t pixel = *reinterpret_cast<const std::uint16_t*>(packed_data + y * byte_width + x * 2);
                            std::uint8_t r = static_cast<std::uint8_t>((pixel & 0xF800) >> 8);
                            r += r >> 5;

                            std::uint8_t g = static_cast<std::uint8_t>((pixel & 0x07E0) >> 3);
                            g += g >> 6;

                            std::uint8_t b = static_cast<std::uint8_t>((pixel & 0x001F) << 3);
                            b += b >> 5;

                            file.write(reinterpret_cast<const char*>(&b), 1);
                            file.write(reinterpret_cast<const char*>(&g), 1);
                            file.write(reinterpret_cast<const char*>(&r), 1);
                        }

                        const std::size_t fill_align = common::align(bitmap->header_.size_pixels.x * 3, 4) 
                            - bitmap->header_.size_pixels.x * 3;
                        
                        const char fill_char = 0;

                        for (std::size_t i = 0; i < fill_align; i++) {
                            file.write(&fill_char, 1);
                        }
                    }

                    break;

                case epoc::display_mode::color256:
                    for (std::size_t y = 0; y < bitmap->header_.size_pixels.y; y++) {
                        for (std::size_t x = 0; x < bitmap->header_.size_pixels.x; x++) {
                            const std::uint8_t pixel = *reinterpret_cast<const std::uint8_t*>(packed_data + y * byte_width + x);
                            std::uint32_t palette_color = epoc::color_256_palette[pixel];

                            file.write(reinterpret_cast<const char*>(&palette_color) + 2, 1);
                            file.write(reinterpret_cast<const char*>(&palette_color) + 1, 1);
                            file.write(reinterpret_cast<const char*>(&palette_color) + 0, 1);
                        }

                        const std::size_t fill_align = common::align(bitmap->header_.size_pixels.x * 3, 4) 
                            - bitmap->header_.size_pixels.x * 3;
                        
                        const char fill_char = 0;

                        for (std::size_t i = 0; i < fill_align; i++) {
                            file.write(&fill_char, 1);
                        }
                    }

                    break;

                case epoc::display_mode::gray256:
                    for (std::size_t y = 0; y < bitmap->header_.size_pixels.y; y++) {
                        for (std::size_t x = 0; x < bitmap->header_.size_pixels.x; x++) {   
                            const std::uint8_t pixel = *reinterpret_cast<const std::uint8_t*>(packed_data + y * byte_width + x);

                            file.write(reinterpret_cast<const char*>(&pixel), 1);
                            file.write(reinterpret_cast<const char*>(&pixel), 1);
                            file.write(reinterpret_cast<const char*>(&pixel), 1);
                        }

                        const std::size_t fill_align = common::align(bitmap->header_.size_pixels.x * 3, 4) 
                            - bitmap->header_.size_pixels.x * 3;
                        
                        const char fill_char = 0;

                        for (std::size_t i = 0; i < fill_align; i++) {
                            file.write(&fill_char, 1);
                        }
                    }

                    break;

                default:    
                    file.write(bitmap->data_offset_ + base, dib_header.uncompressed_size);
                    break;
                }
            }

            return true;
        }
    }

    void fbscli::background_compress_bitmap(service::ipc_context *ctx) {
        const epoc::handle bmp_handle = *ctx->get_arg<epoc::handle>(0);
        fbsbitmap *bmp = obj_table_.get<fbsbitmap>(bmp_handle);

        if (!bmp) {
            ctx->set_request_status(epoc::error_bad_handle);
            return;
        }

        compress_queue *compressor = server<fbs_server>()->compressor.get();

        if (!compressor) {
            ctx->set_request_status(epoc::error_none);
            return;
        }

        epoc::notify_info notify_for_me;
        notify_for_me.requester = ctx->msg->own_thr;
        notify_for_me.sts = ctx->msg->request_sts;

        // Queue notification
        bmp->compress_done_nof = notify_for_me;

        // Done async. Please kernel dont freak out.
        compressor->compress(bmp);
    }
}
