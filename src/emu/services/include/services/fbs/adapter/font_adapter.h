/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project
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

#include <cstdint>
#include <memory>
#include <vector>

#include <common/vecx.h>
#include <services/fbs/font.h>

namespace eka2l1::epoc::adapter {
    struct character_info {
        std::uint16_t x0;
        std::uint16_t y0;
        std::uint16_t x1;
        std::uint16_t y1;
        float xoff;
        float yoff;
        float xadv;
        float xoff2;
        float yoff2;
    };

    static constexpr std::uint32_t INVALID_FONT_TF_UID = 0xFFFFFFFF;

    /**
     * \brief Base class for adapter.
     */
    class font_file_adapter_base {
    public:
        virtual ~font_file_adapter_base() {}

        // Public so that an adapter presenting other adapters as one typeface
        // (see linked_font_file_adapter) can forward this per glyph.
        virtual std::uint32_t get_glyph_advance(const std::size_t face_index, const std::uint32_t codepoint, const std::uint32_t metric_identifier, const bool vertical = false) = 0;

        virtual bool is_valid() = 0;
        virtual bool vectorizable() const = 0;
        virtual std::uint32_t line_gap(const std::size_t idx, const std::uint32_t metric_identifier) {
            return 0;
        }

        virtual bool get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) = 0;

        /**
         * @brief Rasterize one glyph.
         *
         * @param bmp_type In, the format the caller would like, or
         *                 default_glyph_bitmap for whatever suits the adapter.
         *                 Out, the format it really produced -- an adapter is
         *                 free to ignore the request.
         */
        virtual std::uint8_t *get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier,
            int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type,
            open_font_character_metric &character_metric)
            = 0;

        virtual void free_glyph_bitmap(std::uint8_t *data) = 0;
        virtual glyph_bitmap_type get_output_bitmap_type() const = 0;

        virtual bool does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) = 0;

        /**
         * @brief   Measure how much atlas space each glyph needs.
         *
         * Packing the glyphs is the caller's job -- the atlas is its buffer,
         * and only it can see every glyph that has to fit -- so an adapter is
         * asked to do the two things only it can: say how big a glyph is, and
         * draw one. A glyph the face cannot supply reports an empty size and
         * must be skipped by the caller.
         *
         * @param   idx                 Index of the face to measure against.
         * @param   codes               The Unicode codepoints to measure.
         * @param   count               How many codepoints `codes` holds.
         * @param   metric_identifier   The size, as this adapter addresses it.
         * @param   sizes               On return, each glyph's size in pixels.
         *
         * @returns True on success.
         */
        virtual bool measure_atlas_glyphs(const std::size_t idx, const int *codes, const std::size_t count,
            const std::uint32_t metric_identifier, eka2l1::vec2 *sizes) = 0;

        /**
         * @brief   Draw glyphs into an atlas, at positions the caller chose.
         *
         * `positions` holds the top-left corner each glyph is to be drawn at,
         * one per codepoint, in the same order as `codes`. They come from the
         * caller's packing of the sizes measure_atlas_glyphs() reported, so an
         * adapter draws where it is told rather than deciding for itself.
         *
         * @param   idx                 Index of the face to draw from.
         * @param   codes               The Unicode codepoints to draw.
         * @param   count               How many codepoints `codes` holds.
         * @param   metric_identifier   The size, as this adapter addresses it.
         * @param   atlas               The atlas buffer to draw into.
         * @param   atlas_size          Its dimensions; the width is also the row stride.
         * @param   positions           Where each glyph goes.
         * @param   info                On return, each glyph's placement and metrics.
         *
         * @returns True on success.
         */
        virtual bool render_atlas_glyphs(const std::size_t idx, const int *codes, const std::size_t count,
            const std::uint32_t metric_identifier, std::uint8_t *atlas, const eka2l1::vec2 atlas_size,
            const eka2l1::vec2 *positions, character_info *info) = 0;

        /**
         * \brief Get total number of font this file consists of.
         * \returns Number of font in this file.
         */
        virtual std::size_t count() = 0;

        /**
         * @brief       Check if the typeface contains a font with this UID, and get its metric if there's.
         * @param       face_index        The index of the face to check the UID against.
         * @param       uid               The UID to be checked.
         * @returns     The metric of font with the given UID.
         */
        virtual std::optional<open_font_metrics> get_metric_with_uid(const std::size_t face_index, const std::uint32_t uid,
            std::uint32_t *metric_identifier) = 0;

        /**
         * @brief       Check if a character exists.
         * @param       codepoint         The Unicode codepoint of the character to check against.
         */
        virtual bool has_character(const std::size_t face_index, const std::int32_t codepoint, const std::uint32_t metric_identifier) = 0;

        /**
         * @brief Fill shaping struct describing the layout of the text using this font with a specific language code.
         * 
         * @param params            The parameter struct, giving clues for laying out the text.
         * @param text              The target text that will be laid out.
         * @param shaping_header    On return, filled shaping info.
         * @param shaping_data      On return, filled shaping data. This contains the position of each glyph in the text, also the total advance.
         */
        virtual bool make_text_shape(const std::size_t face_index, const open_font_shaping_parameter &params, const std::u16string &text, const std::uint32_t metric_identifier, open_font_shaping_header &shaping_header, std::uint8_t *shaping_data);

        /**
         * @brief Retrieve font table's content.
         * 
         * This is only available in supported font types like TrueType.
         * 
         * @param face_index    The index of the face we want to retrieve the font table from.
         * @param tag4          The tag of the table.
         * @param dest          Dest buffer to write's table content. Use NULL to retrieve the table size in dest_size.
         * @param dest_size     The size of the destination buffer if it's not null. On return contains the written size.
         * 
         * @return True on success.
         */
        virtual bool get_table_content(const std::size_t face_index, const std::uint32_t tag4, std::uint8_t *dest,
            std::uint32_t &dest_size) {
            return false;
        }

        /**
         * @brief Get the nearest supported font metrics in accordance to the ideal size.
         * 
         * @param face_index                The index of the face we want to get the nearest font size from.
         * @param targeted_font_size        The ideal font size.
         * @return std::uint16_t            The nearest font metric that this font supported.
         */
        virtual std::optional<open_font_metrics> get_nearest_supported_metric(const std::size_t face_index, const std::uint16_t targeted_font_size,
            std::uint32_t *metric_identifier = nullptr, bool is_design_font_size = true) = 0;

        virtual std::uint8_t get_atlas_bitmap_bits_per_pixel() {
            return 8;
        }
    };

    /**
     * @brief Number of words a monochrome glyph of this size can ever need.
     */
    std::size_t monochrome_glyph_word_count(const std::int32_t width, const std::int32_t height);

    /**
     * @brief Encode 1 bit per pixel scanlines the way Symbian wants a
     *        monochrome glyph bitmap.
     *
     * A run is a mode bit -- 0 for "the single scanline that follows repeats",
     * 1 for "this many scanlines follow verbatim" -- then a four bit count,
     * then the scanline bits, everything least significant bit first. The
     * source is packed the same way, row major, `width * height` bits.
     *
     * @param dest Buffer of at least monochrome_glyph_word_count() words,
     *             zeroed.
     * @returns The number of bits written.
     */
    std::uint32_t compress_monochrome_glyph(const std::uint32_t *source, const std::int32_t width,
        const std::int32_t height, std::uint32_t *dest);

    enum class font_file_adapter_kind {
        none,
        stb,
        gdr,
        freetype
        // Add your new adapter here
    };

    using font_file_adapter_instance = std::unique_ptr<font_file_adapter_base>;

    /**
     * \brief Create a new font file adapter.
     * 
     * \param kind Kind of backend adapter we want to use.
     * \param dat  Font file data.
     * 
     * \returns An instance of the adapter. Null in case of unrecognised kind or failure.
     */
    font_file_adapter_instance make_font_file_adapter(const font_file_adapter_kind kind, std::vector<std::uint8_t> &dat);
}
