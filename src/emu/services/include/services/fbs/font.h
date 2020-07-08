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

#pragma once

#include <common/map.h>

#include <mem/ptr.h>
#include <utils/des.h>

#include <vector>

namespace eka2l1 {
    class fbs_server;
    struct fbscli;
}

namespace eka2l1::epoc {
    /**
     * Quick note: All struct in this file must be accurate with what's on real hardware, about
     * the structure member layout and order. You can add normal method in these structs, but please
     * absolutely do not use virtual methods since it causes vtable adding in, ruining the layout.
     * 
     * Structure with VTable on Symbian is manually added in.
     */

    struct open_font_metrics {
        std::int16_t design_height;
        std::int16_t ascent;
        std::int16_t descent;
        std::int16_t max_height;
        std::int16_t max_depth;
        std::int16_t max_width;
        std::int16_t baseline_correction;
        std::int16_t reserved;
    };

    static_assert(sizeof(open_font_metrics) == 16);

    struct open_font_face_attrib {
        enum {
            bold = 0x1,
            italic = 0x2,
            serif = 0x4,
            mono_width = 0x8
        };

        bufc_static<char16_t, 32> name;
        std::uint32_t coverage[4];
        std::int32_t style;
        std::int32_t reserved;
        bufc_static<char16_t, 32> fam_name;
        bufc_static<char16_t, 32> local_full_name;
        bufc_static<char16_t, 32> local_full_fam_name;
        std::int32_t min_size_in_pixels;
        std::int32_t reserved2;
    };

    enum glyph_bitmap_type : std::int16_t {
        default_glyph_bitmap, ///< High chance (?) is monochrome.
        monochrome_glyph_bitmap, ///< 1 bit per pixel. This thing is usually compressed using RLE.
        antialised_glyph_bitmap, ///< 8 bit per pixel. Standard. Not compressed.
        subpixel_glyph_bitmap,
        four_color_blend_glyph_bitmap,
        undefined_glyph_bitmap,
        antialised_or_monochrome_glyph_bitmap
    };

    struct typeface_info {
        epoc::bufc_static<char16_t, 0x18> name;
        std::uint32_t flags;

        enum {
            tf_propotional = 1,
            tf_serif = 2,
            tf_symbol = 4
        };
    };

    struct typeface_support {
        typeface_info info_;
        std::uint32_t num_heights_;
        std::int32_t min_height_in_twips_;
        std::int32_t max_height_in_twips_;
        std::int32_t is_scalable_;
    };

    struct font_style_base {
        enum {
            italic = 0x1,
            bold = 0x2,
            super = 0x4,
            sub = 0x8
        };

        // 16 bit (high) = bitmap type, font effects = (12 bit) middule, 4 bits low for style
        std::uint32_t flags;

        void reset_flags() {
            flags = 0;
        }

        glyph_bitmap_type get_glyph_bitmap_type() const {
            return static_cast<glyph_bitmap_type>(flags >> 16);
        }

        void set_glyph_bitmap_type(const glyph_bitmap_type bmp_type) {
            flags &= 0xFFFF;
            flags |= (static_cast<std::uint16_t>(bmp_type) << 16);
        }
    };

    struct font_style_v1: public font_style_base {
    };

    struct font_style_v2: public font_style_base {
        eka2l1::ptr<void> reserved1;
        eka2l1::ptr<void> reserved2;
    };

    struct font_spec_base {
        typeface_info tf;
        std::int32_t height;
    };

    struct font_spec_v1 : public font_spec_base {
        font_style_v1 style;
    };

    struct font_spec_v2: public font_spec_base {
        font_style_v2 style;
    };

    enum {
        DEAD_VTABLE = 0xDEAD1AB
    };

    struct alg_style {
        int baseline_offsets_in_pixel;

        enum {
            bold = 0x1,
            italic = 0x2,
            mono = 0x4
        };

        std::uint8_t flags;
        std::uint8_t width_factor;
        std::uint8_t height_factor;
    };

    struct bitmapfont_base {
        eka2l1::ptr<void> vtable;
    };

    struct bitmapfont_v1: public bitmapfont_base {
        font_spec_v1 spec_in_twips;
        alg_style algorithic_style;

        eka2l1::ptr<void> allocator;
        int fontbitmap_offset;

        // Absolute pointer to COpenFont
        eka2l1::ptr<void> openfont;

        std::uint32_t reserved;
        std::uint32_t font_uid;
    };

    struct bitmapfont_v2 : public bitmapfont_base {
        font_spec_v2 spec_in_twips;
        alg_style algorithic_style;

        eka2l1::ptr<void> allocator;
        int fontbitmap_offset;

        // This was not used by Symbian's by default i think
        // Qt generally access this itself by hardcoding offset of this
        eka2l1::ptr<void> openfont;

        std::uint32_t reserved;
        std::uint32_t font_uid;
    };

    struct open_font {
        eka2l1::ptr<void> vtable;
        eka2l1::ptr<void> allocator;

        open_font_metrics metrics;
        eka2l1::ptr<void> sharper;

        std::int32_t font_captial_offset;
        std::int32_t font_max_ascent;
        std::int32_t font_standard_descent;
        std::int32_t font_max_descent;
        std::int32_t font_line_gap;

        // Normally, i have seen code access this field to get font attrib, but that's the case if the font store
        // that creates this object is on the same process as the one who use this font.
        // FBS generally access the attrib by sending an opcode.
        std::int32_t file_offset;
        std::int32_t face_index_offset;
        std::int32_t glyph_cache_offset;
        std::int32_t session_cache_list_offset;

        eka2l1::ptr<void> reserved;
    };

    struct open_font_character_metric {
        std::int16_t width;
        std::int16_t height;
        std::int16_t horizontal_bearing_x;
        std::int16_t horizontal_bearing_y;
        std::int16_t horizontal_advance;
        std::int16_t vertical_bearing_x;
        std::int16_t vertical_bearing_y;
        std::int16_t vertical_advance;
        glyph_bitmap_type bitmap_type;
        std::int16_t reserved;
    };

    static_assert(sizeof(open_font_character_metric) == 20);

    struct open_font_glyph_v3 {
        std::int32_t codepoint; ///< Unicode value of character.
        std::int32_t glyph_index; ///< The index of this glyph.
        open_font_character_metric metric;
        std::int32_t offset; ///< Offset from *this* pointer to the bitmap data.

        void destroy(fbs_server *serv);
    };

    struct open_font_session_cache_entry_v3 : public open_font_glyph_v3 {
        std::int32_t font_offset; ///< Offset of the font that contains this glyph.
    };

    struct open_font_glyph_v2 {
        std::int32_t codepoint; ///< Unicode value of character.
        std::int32_t glyph_index; ///< The index of this glyph.
        open_font_character_metric metric;
        std::int32_t offset; ///< Offset from *this* pointer to the bitmap data.
        std::uint32_t metric_offset; ///< Pointer to metric. This is what the hell moment.

        void destroy(fbs_server *serv);
    };

    struct open_font_session_cache_entry_v2 : public open_font_glyph_v2 {
        std::int32_t font_offset; ///< Offset of the font that contains this glyph.
        std::int32_t last_use; /*< A number that tells last time this was reference.
                                                 The smaller the number is, the less recent it was referenced. */
    };

    enum {
        NORMAL_SESSION_CACHE_ENTRY_COUNT = 512
    };

    struct open_font_glyph_offset_array {
        std::int32_t offset_array_offset; /**< Offset of the array contains font glyph offset, starting from
                                                *this* pointer. */

        std::int32_t offset_array_count; ///< Total entry in the array.

        void init(fbscli *cli, const std::int32_t count);

        /**
         * \brief    Check if the entry at specified index is empty.
         * \returns  True if empty.
         */
        bool is_entry_empty(fbscli *cli, std::int32_t idx);

        /**
         * \brief   Get the pointer to the offset array.
         * \param   client        The client owner of this array.
         * 
         * \returns Pointer to the offset array if available.
         */
        std::int32_t *pointer(fbscli *cli);

        /**
         * \brief   Set the glyph cache entry to specified pointer.
         * 
         * This doesn't care about if the entry is empty or not.
         * 
         * \param   client        The client owner of this array.
         * \param   idx           The index of the glyph in the cache array.
         * \param   cache_entry   The entry to set at specified index.
         * 
         * \returns False if index is out of range.
         */
        bool set_glyph(fbscli *client, const std::int32_t idx, void *cache_entry);

        /**
         * \brief   Get stored glyph cache entry.
         * 
         * \param   client        The client owner of this array.
         * \param   idx The index of the glyph in the cache array.
         * 
         * \returns Null if index is out of range or no glyph is stored at specified index.
         */
        void *get_glyph(fbscli *client, const std::int32_t idx);
    };

    struct open_font_session_cache_base {
        std::int32_t session_handle;
    };

    /**
     * \brief The second(?) version of the session cache. Used on build 94 and lower.
     */
    struct open_font_session_cache_v2 : public open_font_session_cache_base {
        open_font_glyph_offset_array offset_array;
        std::uint32_t last_use_counter{ 0 };

        /**
         * \brief Add a new glyph in the session cache.
         * 
         * The function first looks for any empty slot in the offset array. If there isn't any slot,
         * it will free and overwrite the slot at position (codepoint % NUM_OFFSET_ARRAY_COUNT).
         * 
         * \param cli        The owner of this session cache.
         * \param code       The codepoint of the added glyph.
         * \param the_glyph  Pointer to the glyph.
         */
        void add_glyph(fbscli *cli, const std::uint32_t code, void *the_glyph);

        /**
         * \brief Destroy the session cache, free any glyph allocated before.
         * \param cli The owner of this session cache.
         */
        void destroy(fbscli *cli);
    };

    /**
     * \brief The third(?) version of the session cache. Used on build 95(?) onwards.
     */
    struct open_font_session_cache_v3 : public open_font_session_cache_base {
        std::int64_t random_seed;
        open_font_glyph_offset_array offset_array;

        /**
         * \brief Add a new glyph in the session cache.
         * 
         * The function first looks for any empty slot in the offset array. If there isn't any slot,
         * it will free and overwrite the slot that has not recently been used. The recent usage can be
         * decided through the field last_use of glyph cache v2. The smaller the number is, the less
         * recent it's used.
         * 
         * \param cli        The owner of this session cache.
         * \param code       The codepoint of the added glyph.
         * \param the_glyph  Pointer to the glyph.
         */
        void add_glyph(fbscli *cli, const std::uint32_t code, void *the_glyph);

        /**
         * \brief Destroy the session cache, free any glyph allocated before.
         * \param cli The owner of this session cache.
         */
        void destroy(fbscli *cli);
    };

    /**
     * \brief A link for open font session cache.
     * Used below FBS build 95.
     */
    struct open_font_session_cache_link {
        eka2l1::ptr<open_font_session_cache_link> next{ 0 };
        eka2l1::ptr<open_font_session_cache_v2> cache{ 0 };

        /**
         * \brief     Try to find the link that correspond to this client, starting from the link that called
         *            this function. If it doesn't exist, a new link will be created.
         * 
         * \param     cli The client to get/create the link of.
         * \returns   The link correspond to given client.
         */
        open_font_session_cache_link *get_or_create(fbscli *cli);

        /**
         * \brief    Delete the link correspond to given client, with search starts from the link called this.
         * \param    cli Pointer to the client that we want to delete the link correspond to it.
         * \returns  True on success.
         */
        bool remove(fbscli *cli);
    };

    enum {
        NORMAL_SESSION_CACHE_LIST_ENTRY_COUNT = 256
    };

    /**
     * \brief A vector map storing open font session cache offset.
     * Used from FBS build 95 onwards.
     */
    struct open_font_session_cache_list : public common::vector_static_map<std::int32_t, std::int32_t,
                                              NORMAL_SESSION_CACHE_ENTRY_COUNT> {
        open_font_session_cache_v3 *get(fbscli *cli, const std::int32_t session_handle, const bool create = false);

        /**
         * \brief    Delete the session cache correspond to given client.
         * \param    cli Pointer to the client that we want to delete the session cache correspond to it.
         * \returns  True on success.
         */
        bool erase_cache(fbscli *cli);
    };
}
