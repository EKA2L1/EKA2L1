/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project.
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

#pragma once

#include <common/uid.h>
#include <loader/mbm.h>
#include <mem/ptr.h>
#include <services/window/common.h>

namespace eka2l1 {
    class fbs_server;

    namespace common {
        class ro_stream;
        class wo_stream;
    }
}

namespace eka2l1::epoc {
    static constexpr epoc::uid bitwise_bitmap_uid = 0x10000040;
    static constexpr std::uint32_t LEGACY_BMP_COMPRESS_IN_MEMORY_TYPE_BASE = 50;
    
    static constexpr std::uint32_t NORMAL_BITMAP_UID_REV2 = 0x9A2C;
    static constexpr std::uint32_t NVG_BITMAP_UID_REV2 = 0x39B9273E;

    static const std::uint32_t SUPPORTED_REV2_UIDS[] = {
        NVG_BITMAP_UID_REV2
    };

    static const std::size_t SUPPORTED_REV2_UID_COUNT = sizeof(SUPPORTED_REV2_UIDS) / sizeof(std::uint32_t);

    // Which NVG extended bitmaps we swap for plain rasterised pixels so guest-side
    // BitGDI can read them (see fbs_server::rasterize_nvg_bitmap).
    //
    // On a device the pixels come from the licensee's CFbsRasterizer plugin, which
    // BitGDI consults for *any* extended bitmap it has to read, rasterising into the
    // bitmap's own TBitmapDesc::iSizeInPixels and iDispMode. There is no notion there
    // of some extended bitmaps being readable and others not, so the only conditions
    // we impose are our own rasteriser's limits.
    static constexpr std::size_t NVG_RASTERIZE_MAX_BYTES = 4 * 1024 * 1024;

    static inline bool is_nvg_bitmap_rasterizable(const eka2l1::vec2 &size, const display_mode dpm) {
        if ((size.x <= 0) || (size.y <= 0)) {
            return false;
        }

        const int bpp = get_bpp_from_display_mode(dpm);

        // Whole-byte pixels only: rasterize_nvg_bitmap stores one pixel at a time and
        // has no packing for the sub-byte modes.
        if ((bpp != 8) && (bpp != 16) && (bpp != 24) && (bpp != 32)) {
            return false;
        }

        // Unlike a real rasterizer, which keeps its pixels in a cache of its own, we
        // expand the bitmap in place and so have to reserve room for the raster form
        // up front (see fbscli::create_bitmap). Cap that reservation: a skin graphic
        // is screen sized at most, while the conceptual size Symbian itself allows an
        // extended bitmap runs up to KMaxTInt / 4 per side.
        return (get_byte_width(size.x, bpp) * static_cast<std::size_t>(size.y)) <= NVG_RASTERIZE_MAX_BYTES;
    }

    enum bitmap_file_compression {
        bitmap_file_no_compression = 0,
        bitmap_file_byte_rle_compression = 1,
        bitmap_file_twelve_bit_rle_compression = 2,
        bitmap_file_sixteen_bit_rle_compression = 3,
        bitmap_file_twenty_four_bit_rle_compression = 4,
        bitmap_file_thirty_two_u_bit_rle_compression = 5,
        bitmap_file_thirty_two_a_bit_rle_compression = 6,
        bitmap_file_palette_compression = 7
    };

    enum bitmap_color {
        monochrome_bitmap = 0,
        color_bitmap = 1,
        color_bitmap_with_alpha = 2,
        color_bitmap_with_alpha_pm = 3
    };

    struct bitwise_bitmap {
        enum settings_flag {
            large_bitmap = 0x00010000,
            dirty_bitmap = 0x00010000,
            violate_bitmap = 0x00020000
        };

        uid uid_;

        struct settings {
            // The first 8 bits are reserved for initial display mode
            // The next 8 bits are reserved for current display mode
            // 16 bits left are for flags, on transition mode this is width for some reason lmao
            std::uint32_t flags_{ 0 };

            display_mode initial_display_mode() const;
            display_mode current_display_mode() const;

            void current_display_mode(const display_mode &mode);
            void initial_display_mode(const display_mode &mode);

            bool is_large() const;
            void set_large(const bool result);

            bool dirty_bitmap() const;
            void dirty_bitmap(const bool is_it);

            bool violate_bitmap() const;
            void violate_bitmap(const bool is_it);

            // LEGACY!
            void set_width(const std::uint16_t bpp);
            std::uint16_t get_width() const;
        } settings_;

        eka2l1::ptr<void> allocator_;
        eka2l1::ptr<void> pile_;
        int byte_width_;
        loader::sbm_header header_;
        int spare1_;
        int data_offset_;
        int compressed_in_ram_;
        bool offset_from_me_;

        void construct(loader::sbm_header &info, epoc::display_mode disp_mode, void *data, const void *base,
            const bool support_current_display_mode_flag, const bool white_fill = false);

        void post_construct(fbs_server *serv);
        int copy_to(std::uint8_t *dest, const eka2l1::vec2 &dest_size, fbs_server *serv);

        bitmap_file_compression compression_type() const;

        std::uint8_t *data_pointer(fbs_server *serv);
        std::uint32_t data_size() const;
    };

    bool save_bwbmp_to_file(const std::string &destination, bitwise_bitmap *bitmap, const char *base);

    bool convert_to_rgba8888(fbs_server *serv, common::ro_stream &source, common::wo_stream &dest, loader::sbm_header &header, std::int32_t byte_width, const bitmap_file_compression comp, const bool make_standard_mask = false);
    bool convert_to_rgba8888(fbs_server *serv, bitwise_bitmap *bmp, common::wo_stream &dest, const bool make_standard_mask = false);
    bool convert_to_rgba8888(fbs_server *serv, loader::mbm_file &file, const std::size_t index, common::wo_stream &dest, const bool make_standard_mask = false);
    /**
     * @brief Composite a Symbian icon mask onto the icon's alpha channel.
     *
     * Both buffers must be RGBA8888 of the same dimensions, and the mask must have
     * been produced by convert_to_rgba8888 with make_standard_mask set. The mask's
     * polarity is worked out from its colour depth and its content; see the
     * implementation for the two families involved.
     *
     * @param icon_rgba     Icon pixels, alpha channel overwritten in place.
     * @param mask_rgba     Mask pixels, read only.
     * @param width         Width in pixels of both buffers.
     * @param height        Height in pixels of both buffers.
     * @param mask_bpp      Colour depth the mask was stored at.
     */
    void apply_icon_mask_alpha(std::uint8_t *icon_rgba, const std::uint8_t *mask_rgba,
        const std::size_t width, const std::size_t height, const std::uint32_t mask_bpp);
}
