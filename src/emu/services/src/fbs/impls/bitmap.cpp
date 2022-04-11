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
#include <common/runlen.h>

#include <services/fbs/fbs.h>
#include <services/fbs/palette.h>
#include <services/fs/fs.h>
#include <services/window/common.h>

#include <kernel/kernel.h>
#include <system/epoc.h>
#include <utils/err.h>
#include <vfs/vfs.h>

#include <algorithm>
#include <cassert>

namespace eka2l1 {
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

    fbs_bitmap_data_info::fbs_bitmap_data_info()
        : dpm_(epoc::display_mode::none)
        , comp_(epoc::bitmap_file_compression::bitmap_file_no_compression)
        , data_(nullptr)
        , data_size_(0) {
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

        bool bitwise_bitmap::settings::is_large() const {
            return flags_ & settings_flag::large_bitmap;
        }

        void bitwise_bitmap::settings::set_large(const bool result) {
            if (result)
                flags_ |= settings_flag::large_bitmap;
            else
                flags_ &= ~(settings_flag::large_bitmap);
        }

        void bitwise_bitmap::settings::set_width(const std::uint16_t w) {
            flags_ &= 0x0000FFFF;
            flags_ |= (static_cast<std::uint32_t>(w) << 16);
        }

        std::uint16_t bitwise_bitmap::settings::get_width() const {
            return static_cast<std::uint16_t>(flags_ >> 16);
        }

        static void do_white_fill(std::uint8_t *dest, const std::size_t size, epoc::display_mode mode) {
            std::fill(dest, dest + size, 0xFF);
        }

        void bitwise_bitmap::construct(loader::sbm_header &info, epoc::display_mode disp_mode, void *data, const void *base, const bool support_current_display_mode_flag, const bool white_fill) {
            uid_ = epoc::BITWISE_BITMAP_UID;
            allocator_ = MAGIC_FBS_HEAP_PTR;
            pile_ = MAGIC_FBS_PILE_PTR;
            byte_width_ = 0;
            header_ = info;
            spare1_ = 0;
            data_offset_ = 0xFFFFFF;
            offset_from_me_ = false;

            if (info.compression != epoc::bitmap_file_no_compression) {
                // Compressed in RAM!
                compressed_in_ram_ = true;
            } else {
                compressed_in_ram_ = false;
            }

            if (data) {
                if (data < base) {
                    data_offset_ = static_cast<int>(reinterpret_cast<const std::uint8_t *>(base) - reinterpret_cast<const std::uint8_t *>(data));

                    data_offset_ *= -1;
                } else {
                    data_offset_ = static_cast<int>(reinterpret_cast<const std::uint8_t *>(data) - reinterpret_cast<const std::uint8_t *>(base));
                }
            } else {
                data_offset_ = 0;
            }

            settings_.initial_display_mode(disp_mode);

            if (support_current_display_mode_flag)
                settings_.current_display_mode(disp_mode);

            byte_width_ = get_byte_width(info.size_pixels.width(), static_cast<std::uint8_t>(info.bit_per_pixels));

            if (white_fill && (data_offset_ != 0)) {
                do_white_fill(reinterpret_cast<std::uint8_t *>(data), info.bitmap_size - sizeof(loader::sbm_header), settings_.current_display_mode());
            }
        }

        void bitwise_bitmap::post_construct(fbs_server *serv) {
            if (serv->legacy_level() >= FBS_LEGACY_LEVEL_EARLY_KERNEL_TRANSITION) {
                if ((header_.compression == epoc::bitmap_file_byte_rle_compression) || (header_.compression == epoc::bitmap_file_twelve_bit_rle_compression))
                    header_.compression += epoc::LEGACY_BMP_COMPRESS_IN_MEMORY_TYPE_BASE;
            }

            // Set large bitmap flag so that the data pointer base is in large chunk
            if (serv->legacy_level() == FBS_LEGACY_LEVEL_EARLY_EKA2) {
                settings_.set_large(offset_from_me_ ? false : true);
            }

            if (serv->legacy_level() == FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
                settings_.set_width(static_cast<std::uint16_t>(header_.size_pixels.x));
            }
        }

        struct bitmap_copy_writer : public common::wo_stream {
            std::uint8_t *dest_;

            std::uint32_t source_byte_width_;
            std::uint32_t dest_byte_width_;
            int max_line_;

            eka2l1::vec2 pos_;

            explicit bitmap_copy_writer(std::uint8_t *dest, const std::uint32_t source_bw, const std::uint32_t dest_bw,
                const int max_line)
                : dest_(dest)
                , source_byte_width_(source_bw)
                , dest_byte_width_(dest_bw)
                , max_line_(max_line) {
            }

            void seek(const std::int64_t amount, common::seek_where wh) override {
                assert(false);
            }

            bool valid() override {
                return (max_line_ < pos_.y);
            }

            std::uint64_t tell() const override {
                return dest_byte_width_ * pos_.y + pos_.x;
            }

            std::uint64_t left() override {
                return max_line_ * dest_byte_width_ - tell();
            }

            std::uint64_t size() override {
                return max_line_ * dest_byte_width_;
            }

            std::uint64_t write(const void *buf, const std::uint64_t write_size) override {
                std::uint64_t consumed = 0;

                const std::uint8_t *buf8 = reinterpret_cast<const std::uint8_t *>(buf);
                const std::uint32_t width_write = common::min<std::uint32_t>(source_byte_width_, dest_byte_width_);

                while (consumed < write_size) {
                    if (pos_.y >= max_line_) {
                        break;
                    }

                    const std::int64_t write_this_line = common::min<std::int64_t>(static_cast<std::int64_t>(write_size - consumed),
                        width_write - pos_.x);

                    std::memcpy(dest_ + pos_.y * dest_byte_width_ + pos_.x, buf8 + consumed, write_this_line);
                    pos_.x += static_cast<int>(write_this_line);

                    if (pos_.x == width_write) {
                        pos_.x = 0;
                        pos_.y++;
                    }

                    consumed += write_this_line;
                }

                return consumed;
            }
        };

        int bitwise_bitmap::copy_to(std::uint8_t *dest, const eka2l1::vec2 &dest_size, fbs_server *serv) {
            const int min_pixel_height = common::min(header_.size_pixels.height(), dest_size.y);
            const int dest_byte_width = get_byte_width(dest_size.x, header_.bit_per_pixels);

            std::uint8_t *src_base = data_pointer(serv);
            std::uint8_t *dest_base = dest;
            std::uint8_t *src = src_base;

            // copy with compressed data not supported yet
            const int min_byte_width = std::min(byte_width_, dest_byte_width);

            if (compressed_in_ram_ && (compression_type() != bitmap_file_no_compression)) {
                bitmap_copy_writer writer(dest, byte_width_, dest_byte_width, min_pixel_height);
                common::ro_buf_stream source_stream(src, data_size());

                switch (compression_type()) {
                case bitmap_file_byte_rle_compression:
                    decompress_rle<8>(reinterpret_cast<common::ro_stream *>(&source_stream), reinterpret_cast<common::wo_stream *>(&writer));
                    break;

                case bitmap_file_twelve_bit_rle_compression:
                    decompress_rle<12>(reinterpret_cast<common::ro_stream *>(&source_stream), reinterpret_cast<common::wo_stream *>(&writer));
                    break;

                case bitmap_file_sixteen_bit_rle_compression:
                    decompress_rle<16>(reinterpret_cast<common::ro_stream *>(&source_stream), reinterpret_cast<common::wo_stream *>(&writer));
                    break;

                case bitmap_file_twenty_four_bit_rle_compression:
                    decompress_rle<24>(reinterpret_cast<common::ro_stream *>(&source_stream), reinterpret_cast<common::wo_stream *>(&writer));
                    break;

                case bitmap_file_thirty_two_a_bit_rle_compression:
                case bitmap_file_thirty_two_u_bit_rle_compression:
                    decompress_rle<32>(reinterpret_cast<common::ro_stream *>(&source_stream), reinterpret_cast<common::wo_stream *>(&writer));
                    break;

                default:
                    LOG_ERROR(SERVICE_FBS, "Unknown compression type {} to get original data for resize", static_cast<int>(compression_type()));
                    break;
                }
            } else {
                for (int row = 0; row < min_pixel_height; row++) {
                    std::memcpy(dest, src, min_byte_width);
                    dest += dest_byte_width;
                    src += byte_width_;
                }
            }

            if (dest_byte_width > byte_width_) {
                const int extra_bits = (header_.size_pixels.width() * header_.bit_per_pixels) & 31;
                if (extra_bits > 0) {
                    uint32_t mask = 0xFFFFFFFF;
                    mask <<= extra_bits;
                    const int dest_word_width = dest_byte_width >> 2;
                    const int src_word_width = byte_width_ >> 2;
                    uint32_t *mask_addr = reinterpret_cast<uint32_t *>(dest_base) + src_word_width - 1;
                    for (int row = 0; row < min_pixel_height; row++) {
                        *mask_addr |= mask;
                        mask_addr += dest_word_width;
                    }
                }
            }

            return epoc::error_none;
        }

        bitmap_file_compression bitwise_bitmap::compression_type() const {
            if (header_.compression >= epoc::LEGACY_BMP_COMPRESS_IN_MEMORY_TYPE_BASE) {
                return static_cast<bitmap_file_compression>(header_.compression - epoc::LEGACY_BMP_COMPRESS_IN_MEMORY_TYPE_BASE);
            }

            return static_cast<bitmap_file_compression>(header_.compression);
        }

        std::uint8_t *bitwise_bitmap::data_pointer(fbs_server *ss) {
            // Use traditional method for on-rom bitmap that does not have additional info
            if (!allocator_ || !pile_ || !ss->is_large_bitmap(header_.bitmap_size - sizeof(loader::sbm_header))) {
                return reinterpret_cast<std::uint8_t *>(this) + data_offset_;
            }

            return ss->get_large_chunk_base() + data_offset_;
        }

        std::uint32_t bitwise_bitmap::data_size() const {
            return header_.bitmap_size - header_.header_len;
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

    static_assert(sizeof(bmp_handles) == 12);

    struct bmp_specs {
        eka2l1::vec2 size;
        epoc::display_mode bpp; // Ignore
        std::uint32_t handle;
        std::uint32_t server_handle;
        std::uint32_t address_offset;
    };

    // Used in Belle and Anna
    struct bmp_specs_v2 {
        eka2l1::vec2 size_pixels;
        eka2l1::vec2 size_twips;
        epoc::display_mode bpp; // Ignore
        std::uint32_t bitmap_uid;
        std::uint32_t data_size_force;
    };

    static_assert(sizeof(bmp_specs_v2) == 28);

    struct bmp_specs_legacy {
        eka2l1::vec2 size;
        epoc::display_mode bpp; // Ignore
        std::uint32_t padding[130];
        std::uint32_t handle;
        std::uint32_t server_handle;
        std::uint32_t address_offset;
    };

    fbsbitmap::~fbsbitmap() {
        if (serv_)
            serv_->free_bitmap(this);
    }

    fbsbitmap *fbsbitmap::final_clean() {
        fbsbitmap *start = this;
        while (start->clean_bitmap) {
            start = start->clean_bitmap;
        }

        return start;
    }

    void *fbs_server::load_data_to_rom(loader::mbm_file &mbmf_, const std::size_t idx_, std::size_t &size_decomp, int *err_code) {
        *err_code = fbs_load_data_err_none;
        size_decomp = 0;

        if (idx_ >= mbmf_.trailer.count) {
            *err_code = fbs_load_data_err_invalid_arg;
            return nullptr;
        }

        std::size_t size_when_compressed = epoc::get_byte_width(mbmf_.sbm_headers[idx_].size_pixels.x,
                                               mbmf_.sbm_headers[idx_].bit_per_pixels)
            * mbmf_.sbm_headers[idx_].size_pixels.y;

        if (!mbmf_.read_single_bitmap(idx_, nullptr, size_when_compressed)) {
            *err_code = fbs_load_data_err_read_decomp_fail;
            return nullptr;
        } else {
            size_decomp = size_when_compressed;
        }

        // Allocates from the large chunk
        // Align them with 4 bytes
        std::size_t avail_dest_size = common::align(size_when_compressed, 4);
        void *data = nullptr;

        if (is_large_bitmap(static_cast<std::uint32_t>(avail_dest_size))) {
            data = large_chunk_allocator->allocate(avail_dest_size);
        } else {
            data = shared_chunk_allocator->allocate(avail_dest_size);
            *err_code = fbs_load_data_err_small_bitmap;
        }

        if (data == nullptr) {
            // We can't allocate at all
            *err_code = fbs_load_data_err_out_of_mem;
            return nullptr;
        }

        // Yay, we manage to alloc memory to load the data in
        // So let's get to work
        bool result_read = mbmf_.read_single_bitmap(idx_, reinterpret_cast<std::uint8_t *>(data), avail_dest_size);

        if (!result_read) {
            *err_code = fbs_load_data_err_read_decomp_fail;
            return nullptr;
        }

        return data;
    }

    void fbscli::load_bitmap(service::ipc_context *ctx) {
        // Get the FS session
        session_ptr fs_target_session = ctx->sys->get_kernel_system()->get<service::session>(*(ctx->get_argument_value<std::int32_t>(2)));
        const std::uint32_t fs_file_handle = *(ctx->get_argument_value<std::uint32_t>(3));

        auto fs_server = reinterpret_cast<eka2l1::fs_server *>(server<fbs_server>()->fs_server);
        file *source_file = fs_server->get_file(fs_target_session->unique_id(), fs_file_handle);

        if (!source_file) {
            ctx->complete(epoc::error_argument);
            return;
        }

        load_bitmap_impl(ctx, source_file);
    }

    struct fast_bitmap_load_info_kerntrans {
        epoc::filename name_;
        std::int32_t id_;
        std::int32_t share_if_loaded_;
        std::uint32_t file_offset_;
    };

    void fbscli::load_bitmap_fast(service::ipc_context *ctx) {
        fbs_server *serv = server<fbs_server>();
        int name_slot = 2;

        std::optional<std::u16string> name = std::nullopt;

        if (serv->legacy_level() >= FBS_LEGACY_LEVEL_S60V1) {
            name_slot = 1;
        } else if (serv->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
            std::optional<fast_bitmap_load_info_kerntrans> load_info = ctx->get_argument_data_from_descriptor<fast_bitmap_load_info_kerntrans>(1);
            if (!load_info.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            name = load_info->name_.to_std_string(nullptr);
            name_slot = -1;
        }

        if (name_slot != -1) {
            name = ctx->get_argument_value<std::u16string>(name_slot);

            if (!name.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }
        }

        symfile source_file = ctx->sys->get_io_system()->open_file(name.value(), READ_MODE | BIN_MODE);

        if (!source_file) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        load_bitmap_impl(ctx, source_file.get());
    }

    void fbscli::duplicate_bitmap(service::ipc_context *ctx) {
        fbsbitmap *bmp = server<fbs_server>()->get<fbsbitmap>(*(ctx->get_argument_value<std::uint32_t>(0)));
        if (!bmp) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        bmp = get_clean_bitmap(bmp);

        const std::uint32_t handle_ret = obj_table_.add(bmp);
        const std::uint32_t server_handle = bmp->id;
        const std::uint32_t off = server<fbs_server>()->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        const bool legacy_return = (ctx->get_argument_data_size(1) >= sizeof(bmp_specs_legacy));

        if (legacy_return) {
            bmp_specs_legacy specs;

            specs.handle = handle_ret;
            specs.server_handle = server_handle;
            specs.address_offset = off;

            ctx->write_data_to_descriptor_argument(1, specs);
        } else {
            bmp_handles handle_info;

            // Add this object to the object table!
            handle_info.handle = handle_ret;
            handle_info.server_handle = server_handle;
            handle_info.address_offset = off;

            ctx->write_data_to_descriptor_argument(1, handle_info);
        }

        ctx->complete(epoc::error_none);
    }

    void fbscli::load_bitmap_impl(service::ipc_context *ctx, file *source) {
        std::optional<load_bitmap_arg> load_options = std::nullopt;
        fbs_server *serv = server<fbs_server>();

        if (serv->legacy_level() >= FBS_LEGACY_LEVEL_S60V1) {
            load_options = std::make_optional<load_bitmap_arg>();
            load_options->bitmap_id = ctx->get_argument_value<std::uint32_t>(2).value();
            load_options->share = ctx->get_argument_value<std::uint32_t>(3).value();
        } else if (serv->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
            std::optional<fast_bitmap_load_info_kerntrans> load_info = ctx->get_argument_data_from_descriptor<fast_bitmap_load_info_kerntrans>(1);
            if (!load_info.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            load_options = std::make_optional<load_bitmap_arg>();
            load_options->bitmap_id = load_info->id_;
            load_options->share = load_info->share_if_loaded_;
            load_options->file_offset = load_info->file_offset_;
        } else {
            load_options = ctx->get_argument_data_from_descriptor<load_bitmap_arg>(1);
        }

        if (!load_options) {
            ctx->complete(epoc::error_argument);
            return;
        }

        LOG_TRACE(SERVICE_FBS, "Loading bitmap from: {}, id: {}", common::ucs2_to_utf8(source->file_name()),
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
            mbmf_.index_to_loads.push_back(load_options->bitmap_id);

            if (!mbmf_.do_read_headers()) {
                ctx->complete(epoc::error_corrupt);
                return;
            }

            // Let's do an insanity check. Is the bitmap index client given us is not valid ?
            if (!mbmf_.is_header_loaded(load_options->bitmap_id)) {
                ctx->complete(epoc::error_not_found);
                return;
            }

            // With doing that, we can now finally start loading to bitmap properly. So let's do it,
            // hesistate is bad.
            epoc::bitwise_bitmap *bws_bmp = fbss->allocate_general_data<epoc::bitwise_bitmap>();

            // Load the bitmap data to large chunk
            int err_code = fbs_load_data_err_none;
            std::size_t size_when_decomp = 0;

            auto bmp_data = fbss->load_data_to_rom(mbmf_, load_options->bitmap_id, size_when_decomp, &err_code);
            std::uint8_t *bmp_data_base = fbss->get_large_chunk_base();

            switch (err_code) {
            case fbs_load_data_err_none:
                break;

            case fbs_load_data_err_small_bitmap:
                bmp_data_base = reinterpret_cast<std::uint8_t *>(bws_bmp);
                break;

            case fbs_load_data_err_out_of_mem: {
                LOG_ERROR(SERVICE_FBS, "Can't allocate data for storing bitmap!");
                ctx->complete(epoc::error_no_memory);

                return;
            }

            case fbs_load_data_err_read_decomp_fail: {
                LOG_ERROR(SERVICE_FBS, "Can't read or decompress bitmap data, possibly corrupted.");
                ctx->complete(epoc::error_corrupt);

                return;
            }

            default: {
                LOG_ERROR(SERVICE_FBS, "Unknown error code from loading uncompressed bitmap!");
                ctx->complete(epoc::error_general);

                return;
            }
            }

            // Get display mode
            loader::sbm_header &header_to_give = mbmf_.sbm_headers[load_options->bitmap_id];
            const epoc::display_mode dpm = epoc::get_display_mode_from_bpp(header_to_give.bit_per_pixels, header_to_give.color);

            bws_bmp->construct(header_to_give, dpm, bmp_data, bmp_data_base, support_current_display_mode, false);
            bws_bmp->offset_from_me_ = (err_code == fbs_load_data_err_small_bitmap);

            bws_bmp->header_.bitmap_size = static_cast<std::uint32_t>(bws_bmp->header_.header_len + size_when_decomp);

            bws_bmp->header_.compression = epoc::bitmap_file_no_compression;
            bws_bmp->post_construct(fbss);

            bmp = make_new<fbsbitmap>(fbss, bws_bmp, static_cast<bool>(load_options->share), support_dirty_bitmap);
        }

        if (load_options->share && !already_cache) {
            fbss->shared_bitmaps.emplace(cache_info_, bmp);
        }

        // Now writes the bitmap info in
        bmp_handles handle_info;

        // Add this object to the object table!
        handle_info.handle = obj_table_.add(bmp);
        handle_info.server_handle = bmp->id;
        handle_info.address_offset = fbss->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        ctx->write_data_to_descriptor_argument<bmp_handles>(0, handle_info);
        ctx->complete(epoc::error_none);
    }

    static std::size_t calculate_aligned_bitmap_bytes(const eka2l1::vec2 &size, const epoc::display_mode bpp) {
        if (size.x == 0 || size.y == 0) {
            return 0;
        }

        return epoc::get_byte_width(size.x, epoc::get_bpp_from_display_mode(bpp)) * size.y;
    }

    static std::uint32_t calculate_reserved_each_side(const std::uint32_t height) {
        // Reserve some space in left and right. Observed shows some apps outwrite their
        // available data region, a little bit, hopefully.
        static constexpr std::uint32_t MAXIMUM_RESERVED_HEIGHT = 50;
        static constexpr std::uint32_t PERCENTAGE_RESERVE_HEIGHT_EACH_SIDE = 15;

        return common::min<std::uint32_t>(MAXIMUM_RESERVED_HEIGHT, height * PERCENTAGE_RESERVE_HEIGHT_EACH_SIDE / 100);
    }

    fbsbitmap *fbs_server::create_bitmap(fbs_bitmap_data_info &info, const bool alloc_data, const bool support_current_display_mode_flag, const bool support_dirty) {
        if (!shared_chunk || !large_chunk) {
            initialize_server();
        }

        epoc::bitwise_bitmap *bws_bmp = allocate_general_data<epoc::bitwise_bitmap>();
        std::uint32_t final_reserve_each_side = calculate_reserved_each_side(info.size_.y);

        // Data size fixed
        if (info.data_size_ || !alloc_data) {
            final_reserve_each_side = 0;
        }

        // Calculate the size
        const std::size_t byte_width = epoc::get_byte_width(info.size_.x, epoc::get_bpp_from_display_mode(info.dpm_));

        std::size_t original_bytes = (info.data_size_ == 0) ? calculate_aligned_bitmap_bytes(
                                         info.size_, info.dpm_)
                                                            : info.data_size_;

        std::size_t alloc_bytes = original_bytes + final_reserve_each_side * byte_width * 2;

        void *data = nullptr;
        std::uint8_t *base = nullptr;
        bool smol = false;

        if ((original_bytes > 0) && alloc_data) {
            // Allocates from the large chunk
            // Align them with 4 bytes
            std::size_t org_dest_size = common::align(original_bytes, 4);
            std::size_t avail_dest_size = common::align(alloc_bytes, 4);

            if (!is_large_bitmap(static_cast<std::uint32_t>(org_dest_size))) {
                base = reinterpret_cast<std::uint8_t *>(bws_bmp);
                data = shared_chunk_allocator->allocate(avail_dest_size);

                smol = true;
            } else {
                base = base_large_chunk;
                data = large_chunk_allocator->allocate(avail_dest_size);
            }

            if (!data) {
                shared_chunk_allocator->freep(bws_bmp);
                return nullptr;
            }
        }

        loader::sbm_header header;
        header.compression = info.comp_;
        header.bitmap_size = static_cast<std::uint32_t>(original_bytes + sizeof(loader::sbm_header));
        header.size_pixels = info.size_;
        header.color = get_bitmap_color_type_from_display_mode(info.dpm_);
        header.header_len = sizeof(loader::sbm_header);
        header.palette_size = 0;
        header.size_twips = info.size_ * epoc::get_approximate_pixel_to_twips_mul(kern->get_epoc_version());
        header.bit_per_pixels = epoc::get_bpp_from_display_mode(info.dpm_);

        // Skip reserved spaces.
        const std::size_t byte_width_calc = epoc::get_byte_width(header.size_pixels.width(), static_cast<std::uint8_t>(header.bit_per_pixels));
        const std::size_t reserved_bytes = (byte_width_calc * final_reserve_each_side);
        if (data) {
            data = reinterpret_cast<std::uint8_t *>(data) + reserved_bytes;
        }

        bws_bmp->construct(header, info.dpm_, data, base, support_current_display_mode_flag, true);
        bws_bmp->offset_from_me_ = smol;
        bws_bmp->post_construct(this);

        if (info.data_) {
            std::memcpy(data, info.data_, common::min<std::size_t>(info.data_size_, original_bytes));
        }

        fbsbitmap *bmp = make_new<fbsbitmap>(this, bws_bmp, false, support_dirty, final_reserve_each_side);
        return bmp;
    }

    bool fbs_server::free_bitmap(fbsbitmap *bmp) {
        if (!bmp->bitmap_ || bmp->count > 0) {
            return false;
        }

        bool no_failure = true;

        if (bmp->bitmap_->data_offset_) {
            const std::size_t reserved_bytes = bmp->reserved_height_each_side_ * bmp->bitmap_->byte_width_;

            // First, free the bitmap pixels.
            if (bmp->bitmap_->offset_from_me_) {
                shared_chunk_allocator->freep(bmp->bitmap_->data_pointer(this) - reserved_bytes);
                no_failure = true;
            } else {
                large_chunk_allocator->freep(bmp->bitmap_->data_pointer(this) - reserved_bytes);
                no_failure = true;
            }
        }

        // Free the bitwise bitmap.
        if (!free_general_data(bmp->bitmap_)) {
            no_failure = true;
        }

        common::erase_elements(shared_bitmaps, [bmp](const std::pair<const eka2l1::fbsbitmap_cache_info, fbsbitmap*> &info) -> bool {
            return info.second == bmp;
        });

        return no_failure;
    }

    bool fbs_server::is_large_bitmap(const std::uint32_t compressed_size) const {
        static constexpr std::uint32_t RANGE_START_LARGE = 1 << 12;
        static constexpr std::uint32_t RANGE_START_LARGE_TRANS = 1 << 16;

        const std::uint32_t size_aligned = (((compressed_size + 3) / 4) << 2);

        switch (legacy_level()) {
        case FBS_LEGACY_LEVEL_S60V1:
        case FBS_LEGACY_LEVEL_EARLY_KERNEL_TRANSITION:
            return size_aligned >= RANGE_START_LARGE;

        case FBS_LEGACY_LEVEL_KERNEL_TRANSITION:
            return size_aligned >= RANGE_START_LARGE_TRANS;

        default:
            break;
        }

        return true;
    }

    void fbscli::create_bitmap(service::ipc_context *ctx) {
        bmp_specs_legacy specs;

        const bool use_spec_legacy = ctx->get_argument_data_size(0) >= sizeof(bmp_specs_legacy);
        const bool use_bmp_handles_writeback = (server<fbs_server>()->get_system()->get_symbian_version_use()
            >= epocver::epoc10);

        if (!use_spec_legacy) {
            if (use_bmp_handles_writeback) {
                std::optional<bmp_specs_v2> specs_morden_v2 = ctx->get_argument_data_from_descriptor<bmp_specs_v2>(0);
                if (!specs_morden_v2.has_value()) {
                    ctx->complete(epoc::error_argument);
                    return;
                }

                specs.size = specs_morden_v2->size_pixels;
                specs.bpp = specs_morden_v2->bpp;

                static constexpr std::uint32_t NORMAL_BITMAP_UID_REV2 = 0x9A2C;

                if (specs_morden_v2->bitmap_uid != NORMAL_BITMAP_UID_REV2) {
                    LOG_WARN(SERVICE_FBS, "Trying to create non-standard bitmap with UID 0x{:X}! Revisit soon!", specs_morden_v2->bitmap_uid);
                }
            } else {
                std::optional<bmp_specs> specs_morden = ctx->get_argument_data_from_descriptor<bmp_specs>(0);

                if (!specs_morden) {
                    ctx->complete(epoc::error_argument);
                    return;
                }

                specs.size = specs_morden->size;
                specs.bpp = specs_morden->bpp;
            }
        } else {
            std::optional<bmp_specs_legacy> specs_legacy = ctx->get_argument_data_from_descriptor<bmp_specs_legacy>(0);

            if (!specs_legacy) {
                ctx->complete(epoc::error_argument);
                return;
            }

            specs = std::move(specs_legacy.value());
        }

        fbs_server *fbss = server<fbs_server>();

        fbs_bitmap_data_info info;
        info.size_ = specs.size;
        info.dpm_ = specs.bpp;

        fbsbitmap *bmp = fbss->create_bitmap(info, true, support_current_display_mode, support_dirty_bitmap);

        if (!bmp) {
            ctx->complete(epoc::error_no_memory);
            return;
        }

        const std::uint32_t handle_ret = obj_table_.add(bmp);
        const std::uint32_t serv_handle = bmp->id;
        const std::uint32_t addr_off = fbss->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        // From Anna the slot to write this moved to 1.

        if (use_spec_legacy) {
            specs.handle = handle_ret;
            specs.server_handle = serv_handle;
            specs.address_offset = addr_off;

            ctx->write_data_to_descriptor_argument(0, specs);
        } else {
            if (use_bmp_handles_writeback) {
                bmp_handles handles;
                handles.address_offset = addr_off;
                handles.server_handle = serv_handle;
                handles.handle = handle_ret;

                ctx->write_data_to_descriptor_argument<bmp_handles>(1, handles);
            } else {
                bmp_specs specs_to_write;
                specs_to_write.size = specs.size;
                specs_to_write.bpp = specs.bpp;

                specs_to_write.handle = handle_ret;
                specs_to_write.server_handle = serv_handle;
                specs_to_write.address_offset = addr_off;

                ctx->write_data_to_descriptor_argument(0, specs_to_write);
            }
        }

        ctx->complete(epoc::error_none);
    }

    fbsbitmap *fbscli::get_clean_bitmap(fbsbitmap *bmp) {
        return bmp->final_clean();
    }

    void fbscli::resize_bitmap(service::ipc_context *ctx) {
        const auto fbss = server<fbs_server>();
        const epoc::handle handle = *(ctx->get_argument_value<std::uint32_t>(0));

        fbsbitmap *bmp = obj_table_.get<fbsbitmap>(handle);

        if (!bmp) {
            ctx->complete(epoc::error_unknown);
            return;
        }

        bmp = get_clean_bitmap(bmp);

        const vec2 new_size = { *(ctx->get_argument_value<int>(1)), *(ctx->get_argument_value<int>(2)) };

        fbsbitmap *new_bmp = nullptr;
        std::uint8_t *dest_data = nullptr;
        std::uint8_t *base = nullptr;

        const std::uint32_t reserved_each_size = calculate_reserved_each_side(new_size.y);
        bool offset_from_me_now = false;

        if ((new_size.x != 0) && (new_size.y != 0)) {
            if (fbss->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
                new_bmp = bmp;

                const int dest_byte_width = epoc::get_byte_width(new_size.x, bmp->bitmap_->header_.bit_per_pixels);
                const int size_total = dest_byte_width * new_size.y;
                const int size_added_reserve = size_total + reserved_each_size * dest_byte_width * 2;

                if (fbss->is_large_bitmap(size_total)) {
                    dest_data = reinterpret_cast<std::uint8_t *>(fbss->allocate_large_data(size_added_reserve));
                    base = fbss->get_large_chunk_base();
                } else {
                    dest_data = reinterpret_cast<std::uint8_t *>(fbss->allocate_general_data_impl(size_added_reserve));
                    base = reinterpret_cast<std::uint8_t *>(new_bmp->bitmap_);

                    offset_from_me_now = true;
                }

                dest_data += new_bmp->reserved_height_each_side_ * dest_byte_width;
            } else {
                const epoc::display_mode disp_mode = bmp->bitmap_->settings_.current_display_mode();

                fbs_bitmap_data_info info;
                info.size_ = new_size;
                info.dpm_ = disp_mode;

                new_bmp = fbss->create_bitmap(info, true, support_current_display_mode, support_dirty_bitmap);
                dest_data = new_bmp->bitmap_->data_pointer(fbss);
            }

            bmp->bitmap_->copy_to(dest_data, new_size, fbss);
        } else {
            dest_data = nullptr;
            base = nullptr;

            if (fbss->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
                new_bmp = bmp;
                offset_from_me_now = true;
            } else {
                const epoc::display_mode disp_mode = bmp->bitmap_->settings_.current_display_mode();

                fbs_bitmap_data_info info;
                info.size_ = new_size;
                info.dpm_ = disp_mode;

                new_bmp = fbss->create_bitmap(info, false, support_current_display_mode, support_dirty_bitmap);
            }
        }

        if (fbss->legacy_level() <= FBS_LEGACY_LEVEL_EARLY_EKA2) {
            bmp->clean_bitmap = new_bmp;
            bmp->bitmap_->settings_.dirty_bitmap(true);

            new_bmp->ref();
            new_bmp->ref_extra_ed = true;

            obj_table_.remove(handle);
            bmp_handles handle_info;

            // Add this object to the object table!
            handle_info.handle = obj_table_.add(new_bmp);
            handle_info.server_handle = new_bmp->id;
            handle_info.address_offset = server<fbs_server>()->host_ptr_to_guest_shared_offset(new_bmp->bitmap_);

            ctx->write_data_to_descriptor_argument(3, handle_info);

            // Notify dirty
            if (fbss->compressor)
                fbss->compressor->finish_notify(epoc::error_none);
        } else {
            loader::sbm_header old_header = new_bmp->bitmap_->header_;
            old_header.size_pixels.x = new_size.x;
            old_header.size_pixels.y = new_size.y;
            old_header.size_twips = old_header.size_pixels * epoc::get_approximate_pixel_to_twips_mul(fbss->kern->get_epoc_version());

            // Free old data
            std::uint8_t *data = new_bmp->original_pointer(fbss);
            if (new_bmp->bitmap_->offset_from_me_) {
                fbss->free_general_data_impl(data);
            } else {
                fbss->free_large_data(data);
            }

            new_bmp->reserved_height_each_side_ = reserved_each_size;
            new_bmp->bitmap_->construct(old_header, new_bmp->bitmap_->settings_.initial_display_mode(),
                dest_data, base, support_current_display_mode, false);
            new_bmp->bitmap_->post_construct(fbss);
        }

        new_bmp->bitmap_->offset_from_me_ = offset_from_me_now;
        ctx->complete(epoc::error_none);
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

        ctx->complete(epoc::error_none);
    }

    void fbscli::get_clean_bitmap(service::ipc_context *ctx) {
        const epoc::handle bmp_handle = *ctx->get_argument_value<epoc::handle>(0);
        fbsbitmap *bmp = obj_table_.get<fbsbitmap>(bmp_handle);

        if (!bmp) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        bmp = get_clean_bitmap(bmp);
        bmp_handles handle_info;

        // Get the clean bitmap handle!
        handle_info.handle = obj_table_.add(bmp);
        handle_info.server_handle = bmp->id;
        handle_info.address_offset = server<fbs_server>()->host_ptr_to_guest_shared_offset(bmp->bitmap_);

        // We previously do a ref to prevent duplicate instances from destroying this clean bitmap...
        if (bmp->ref_extra_ed) {
            bmp->deref();
            bmp->ref_extra_ed = false;
        }

        // Close the old handle. To prevent this object from being destroyed.
        // In case no clean bitmap at all!
        obj_table_.remove(bmp_handle);

        ctx->write_data_to_descriptor_argument(1, handle_info);
        ctx->complete(epoc::error_none);
    }

    namespace epoc {
        bool save_bwbmp_to_file(const std::string &destination, epoc::bitwise_bitmap *bitmap, const char *base, const epocver sysver) {
            if (bitmap->header_.compression != epoc::bitmap_file_no_compression) {
                return false;
            }

            bool need_process = false;

            common::dib_header_v2 dib_header;

            dib_header.uncompressed_size = static_cast<std::uint32_t>(calculate_aligned_bitmap_bytes(bitmap->header_.size_pixels, bitmap->settings_.current_display_mode()));
            dib_header.bit_per_pixels = bitmap->header_.bit_per_pixels;
            dib_header.color_plane_count = 1;
            dib_header.important_color_count = 0;
            dib_header.palette_count = 0;
            dib_header.size = bitmap->header_.size_pixels;

            switch (dib_header.bit_per_pixels) {
            case 8:
            case 16: {
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
            header.file_size = static_cast<std::uint32_t>(sizeof(common::bmp_header) + dib_header.header_size + dib_header.uncompressed_size);
            header.pixel_array_offset = static_cast<std::uint32_t>(sizeof(common::bmp_header) + dib_header.header_size);

            std::ofstream file(destination);

            if (file.fail()) {
                return false;
            }

            file.write(reinterpret_cast<const char *>(&header), sizeof(common::bmp_header));
            file.write(reinterpret_cast<const char *>(&dib_header), dib_header.header_size);

            if (need_process) {
                const std::uint32_t byte_width = get_byte_width(bitmap->header_.size_pixels.x,
                    bitmap->header_.bit_per_pixels);

                const std::uint8_t *packed_data = reinterpret_cast<const std::uint8_t *>(bitmap->data_offset_ + base);

                switch (bitmap->settings_.current_display_mode()) {
                case epoc::display_mode::color64k:
                    for (std::size_t y = 0; y < bitmap->header_.size_pixels.y; y++) {
                        for (std::size_t x = 0; x < bitmap->header_.size_pixels.x; x++) {
                            const std::uint16_t pixel = *reinterpret_cast<const std::uint16_t *>(packed_data + y * byte_width + x * 2);
                            std::uint8_t r = static_cast<std::uint8_t>((pixel & 0xF800) >> 8);
                            r += r >> 5;

                            std::uint8_t g = static_cast<std::uint8_t>((pixel & 0x07E0) >> 3);
                            g += g >> 6;

                            std::uint8_t b = static_cast<std::uint8_t>((pixel & 0x001F) << 3);
                            b += b >> 5;

                            file.write(reinterpret_cast<const char *>(&b), 1);
                            file.write(reinterpret_cast<const char *>(&g), 1);
                            file.write(reinterpret_cast<const char *>(&r), 1);
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
                            const std::uint8_t pixel = *reinterpret_cast<const std::uint8_t *>(packed_data + y * byte_width + x);
                            std::uint32_t palette_color = epoc::get_suitable_palette_256(sysver)[pixel];

                            file.write(reinterpret_cast<const char *>(&palette_color) + 2, 1);
                            file.write(reinterpret_cast<const char *>(&palette_color) + 1, 1);
                            file.write(reinterpret_cast<const char *>(&palette_color) + 0, 1);
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
                            const std::uint8_t pixel = *reinterpret_cast<const std::uint8_t *>(packed_data + y * byte_width + x);

                            file.write(reinterpret_cast<const char *>(&pixel), 1);
                            file.write(reinterpret_cast<const char *>(&pixel), 1);
                            file.write(reinterpret_cast<const char *>(&pixel), 1);
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

        bool convert_to_rgba8888(fbs_server *serv, common::ro_stream &source, common::wo_stream &dest, loader::sbm_header &header, std::int32_t byte_width, const bitmap_file_compression comp, const bool make_standard_mask) {
            if (byte_width == -1) {
                byte_width = get_byte_width(header.size_pixels.x, header.bit_per_pixels);
            }

            std::vector<std::uint8_t> decomp_data;
            common::ro_stream *current_to_look = &source;

            std::unique_ptr<common::ro_buf_stream> decomp_data_source_stream;

            if (comp != bitmap_file_no_compression) {
                decomp_data.resize(byte_width * header.size_pixels.y);

                if (header.bitmap_size - header.header_len > common::MB(4)) {
                    common::wo_buf_stream decomp_dest_stream(decomp_data.data(), decomp_data.size());

                    switch (comp) {
                    case bitmap_file_byte_rle_compression:
                        decompress_rle<8>(reinterpret_cast<common::ro_stream *>(&source),
                            reinterpret_cast<common::wo_stream *>(&decomp_dest_stream));
                        break;

                    case bitmap_file_twelve_bit_rle_compression:
                        decompress_rle<12>(reinterpret_cast<common::ro_stream *>(&source),
                            reinterpret_cast<common::wo_stream *>(&decomp_dest_stream));
                        break;

                    case bitmap_file_sixteen_bit_rle_compression:
                        decompress_rle<16>(reinterpret_cast<common::ro_stream *>(&source),
                            reinterpret_cast<common::wo_stream *>(&decomp_dest_stream));
                        break;

                    case bitmap_file_twenty_four_bit_rle_compression:
                    case bitmap_file_thirty_two_u_bit_rle_compression:
                        decompress_rle<24>(reinterpret_cast<common::ro_stream *>(&source),
                            reinterpret_cast<common::wo_stream *>(&decomp_dest_stream));
                        break;

                    case bitmap_file_thirty_two_a_bit_rle_compression:
                        decompress_rle<32>(reinterpret_cast<common::ro_stream *>(&source),
                            reinterpret_cast<common::wo_stream *>(&decomp_dest_stream));

                    default:
                        LOG_ERROR(SERVICE_FBS, "Unsupported compression type {}", header.compression);
                        return false;
                    }
                } else {
                    std::vector<std::uint8_t> source_data(header.bitmap_size - header.header_len);
                    source.read(source_data.data(), source_data.size());

                    std::size_t final_size = decomp_data.size();

                    switch (comp) {
                    case bitmap_file_byte_rle_compression:
                        decompress_rle_fast_route<8>(source_data.data(), source_data.size(), decomp_data.data(), final_size);
                        break;

                    case bitmap_file_twelve_bit_rle_compression:
                        decompress_rle_fast_route<12>(source_data.data(), source_data.size(), decomp_data.data(), final_size);
                        break;

                    case bitmap_file_sixteen_bit_rle_compression:
                        decompress_rle_fast_route<16>(source_data.data(), source_data.size(), decomp_data.data(), final_size);
                        break;

                    case bitmap_file_twenty_four_bit_rle_compression:
                    case bitmap_file_thirty_two_u_bit_rle_compression:
                        decompress_rle_fast_route<24>(source_data.data(), source_data.size(), decomp_data.data(), final_size);
                        break;

                    case bitmap_file_thirty_two_a_bit_rle_compression:
                        decompress_rle_fast_route<32>(source_data.data(), source_data.size(), decomp_data.data(), final_size);
                        break;

                    default:
                        LOG_ERROR(SERVICE_FBS, "Unsupported compression type {}", header.compression);
                        return false;
                    }
                }

                decomp_data_source_stream = std::make_unique<common::ro_buf_stream>(decomp_data.data(), decomp_data.size());
                current_to_look = decomp_data_source_stream.get();
            }

            current_to_look->seek(0, common::seek_where::beg);
            epoc::display_mode dpm = get_display_mode_from_bpp(header.bit_per_pixels, header.color);

            // Unused top byte. Just make it rgb24 then!
            if ((comp == bitmap_file_thirty_two_u_bit_rle_compression) && (dpm == epoc::display_mode::color16ma)) {
                dpm = epoc::display_mode::color16m;
                byte_width = header.size_pixels.x * 3;
            }

            switch (dpm) {
            case epoc::display_mode::color256:
                for (std::size_t y = 0; y < header.size_pixels.y; y++) {
                    current_to_look->seek(y * byte_width, common::seek_where::beg);

                    for (std::size_t x = 0; x < header.size_pixels.x; x++) {
                        std::uint8_t pixel = 0;
                        if (current_to_look->read(&pixel, 1) != 1) {
                            return false;
                        }

                        std::uint32_t palette_color = epoc::get_suitable_palette_256(serv->get_kernel_object_owner()->get_epoc_version())[pixel];

                        std::uint8_t a = ((make_standard_mask) ? ((palette_color & 0xFFFFFF) == 0xFFFFFF) : 1) * 255;

                        dest.write(reinterpret_cast<const char *>(&palette_color) + 0, 1);
                        dest.write(reinterpret_cast<const char *>(&palette_color) + 1, 1);
                        dest.write(reinterpret_cast<const char *>(&palette_color) + 2, 1);
                        dest.write(&a, 1);
                    }
                }

                break;

            case epoc::display_mode::color4k:
                for (std::size_t y = 0; y < header.size_pixels.y; y++) {
                    current_to_look->seek(y * byte_width, common::seek_where::beg);

                    for (std::size_t x = 0; x < header.size_pixels.x; x++) {
                        std::uint16_t pixel = 0;
                        if (current_to_look->read(&pixel, 2) != 2) {
                            return false;
                        }
                        std::uint8_t r = static_cast<std::uint8_t>(((pixel >> 8) & 0xF) * 17);
                        std::uint8_t g = static_cast<std::uint8_t>(((pixel >> 4) & 0xF) * 17);
                        std::uint8_t b = static_cast<std::uint8_t>((pixel & 0xF) * 17);
                        std::uint8_t a = ((make_standard_mask) ? ((r == 255) && (g == 255) && (b == 255)) : 1) * 255;
                        dest.write(&r, 1);
                        dest.write(&g, 1);
                        dest.write(&b, 1);
                        dest.write(&a, 1);
                    }
                }

                break;

            case epoc::display_mode::color64k:
                for (std::size_t y = 0; y < header.size_pixels.y; y++) {
                    current_to_look->seek(y * byte_width, common::seek_where::beg);

                    for (std::size_t x = 0; x < header.size_pixels.x; x++) {
                        std::uint16_t pixel = 0;
                        if (current_to_look->read(&pixel, 2) != 2) {
                            return false;
                        }

                        std::uint8_t r = static_cast<std::uint8_t>((pixel & 0xF800) >> 8);
                        r += r >> 5;

                        std::uint8_t g = static_cast<std::uint8_t>((pixel & 0x07E0) >> 3);
                        g += g >> 6;

                        std::uint8_t b = static_cast<std::uint8_t>((pixel & 0x001F) << 3);
                        b += b >> 5;

                        std::uint8_t a = ((make_standard_mask) ? ((r == 255) && (g == 255) && (b == 255)) : 1) * 255;
                        dest.write(&r, 1);
                        dest.write(&g, 1);
                        dest.write(&b, 1);
                        dest.write(&a, 1);
                    }
                }

                break;

            case epoc::display_mode::color16m:
                for (std::size_t y = 0; y < header.size_pixels.y; y++) {
                    current_to_look->seek(y * byte_width, common::seek_where::beg);

                    for (std::size_t x = 0; x < header.size_pixels.x; x++) {
                        std::uint8_t base[3];
                        if (current_to_look->read(base, 3) != 3) {
                            return false;
                        }

                        std::uint8_t a = ((make_standard_mask) ? ((base[0] == 255) && (base[1] == 255) && (base[2] == 255)) : 1) * 255;
                        dest.write(base + 2, 1);
                        dest.write(base + 1, 1);
                        dest.write(base, 1);
                        dest.write(&a, 1);
                    }
                }

                break;

            case epoc::display_mode::color16ma: {
                for (std::size_t y = 0; y < header.size_pixels.y; y++) {    
                    current_to_look->seek(y * byte_width, common::seek_where::beg);

                    for (std::size_t x = 0; x < header.size_pixels.x; x++) {
                        std::uint8_t base[4];
                        if (current_to_look->read(base, 4) != 3) {
                            return false;
                        }

                        dest.write(base + 2, 1);
                        dest.write(base + 1, 1);
                        dest.write(base, 1);
                        dest.write(base + 3, 1);
                    }
                }

                break;
            }

            case epoc::display_mode::gray256:
                for (std::size_t y = 0; y < header.size_pixels.y; y++) {
                    current_to_look->seek(y * byte_width, common::seek_where::beg);

                    for (std::size_t x = 0; x < header.size_pixels.x; x++) {
                        std::uint8_t pixel = 0;
                        if (current_to_look->read(&pixel, 1) != 1) {
                            return false;
                        }
                        dest.write(&pixel, 1);
                        dest.write(&pixel, 1);
                        dest.write(&pixel, 1);
                        dest.write(&pixel, 1);
                    }
                }

                break;

            case epoc::display_mode::gray2:
                for (std::size_t y = 0; y < header.size_pixels.y; y++) {
                    for (std::size_t x = 0; x < header.size_pixels.x; x++) {
                        std::uint32_t color = 0;
                        current_to_look->seek(y * byte_width + (x / 32) * 4, common::seek_where::beg);
                        if (current_to_look->read(&color, 4) != 4) {
                            return false;
                        }
                        std::uint32_t converted_color = 0;
                        if (color & (1 << (x & 0x1F))) {
                            converted_color = 0xFFFFFFFF;
                        }

                        dest.write(&converted_color, sizeof(converted_color));
                    }
                }

                break;

            default:
                LOG_ERROR(SERVICE_FBS, "Unsupported display mode to convert to ARGB8888 {}", static_cast<int>(dpm));
                return false;
            }

            return true;
        }

        bool convert_to_rgba8888(fbs_server *serv, bitwise_bitmap *bmp, common::wo_stream &dest, const bool make_standard_mask) {
            if (!bmp) {
                return false;
            }

            std::uint8_t *data_ptr = bmp->data_pointer(serv);
            common::ro_buf_stream buf_stream(data_ptr, bmp->data_size());
            return convert_to_rgba8888(serv, buf_stream, dest, bmp->header_, bmp->byte_width_, bmp->compression_type(), make_standard_mask);
        }

        bool convert_to_rgba8888(fbs_server *serv, loader::mbm_file &file, const std::size_t index, common::wo_stream &dest, const bool make_standard_mask) {
            std::size_t max_data = 0;
            if (!file.read_single_bitmap_raw(index, nullptr, max_data)) {
                return false;
            }

            std::vector<std::uint8_t> data(max_data);
            if (!file.read_single_bitmap_raw(index, data.data(), max_data)) {
                return false;
            }

            common::ro_buf_stream buf_stream(data.data(), data.size());
            return convert_to_rgba8888(serv, buf_stream, dest, file.sbm_headers[index], -1, static_cast<bitmap_file_compression>(file.sbm_headers[index].compression), make_standard_mask);
        }
    }

    void fbscli::background_compress_bitmap(service::ipc_context *ctx) {
        const epoc::handle bmp_handle = *ctx->get_argument_value<epoc::handle>(0);
        fbsbitmap *bmp = obj_table_.get<fbsbitmap>(bmp_handle);

        if (!bmp) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        bmp = get_clean_bitmap(bmp);
        compress_queue *compressor = server<fbs_server>()->compressor.get();

        if (!compressor) {
            ctx->complete(epoc::error_none);
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

    void fbscli::compress_bitmap(service::ipc_context *ctx) {
        const epoc::handle bmp_handle = *ctx->get_argument_value<epoc::handle>(0);
        fbsbitmap *bmp = obj_table_.get<fbsbitmap>(bmp_handle);

        if (!bmp) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        bmp = get_clean_bitmap(bmp);
        compress_queue *compressor = server<fbs_server>()->compressor.get();

        if (!compressor) {
            ctx->complete(epoc::error_none);
            return;
        }

        epoc::notify_info notify_for_me;
        notify_for_me.requester = ctx->msg->own_thr;
        notify_for_me.sts = ctx->msg->request_sts;

        // Queue notification
        bmp->compress_done_nof = notify_for_me;
        compressor->actual_compress(bmp);
    }
}
