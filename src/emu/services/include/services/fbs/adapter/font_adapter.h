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

        virtual bool is_valid() = 0;
        virtual bool vectorizable() const = 0;
        virtual std::uint32_t line_gap(const std::size_t idx) {
            return 0;
        }

        virtual bool get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) = 0;
        virtual bool get_metrics(const std::size_t idx, open_font_metrics &metrics) = 0;
        virtual bool get_glyph_metric(const std::size_t idx, std::uint32_t code, open_font_character_metric &metric,
            const std::int32_t baseline_horz_off, const std::uint16_t font_size)
            = 0;

        virtual std::uint8_t *get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint16_t font_size,
            int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type)
            = 0;

        virtual void free_glyph_bitmap(std::uint8_t *data) = 0;
        virtual glyph_bitmap_type get_output_bitmap_type() const = 0;

        virtual bool does_glyph_exist(std::size_t idx, std::uint32_t code) = 0;

        /**
         * @brief   Initialize getting glyph atlas.
         * 
         * Each pixel is 8 bits.
         * 
         * @param   atlas_ptr Pointer to destination data which glyph bitmap will be written to.
         * @param   atlas_size Size of the atlas.
         * 
         * @returns Handle to the atlas get context. -1 on failure.
         */
        virtual std::int32_t begin_get_atlas(std::uint8_t *atlas_ptr, const eka2l1::vec2 atlas_size) = 0;

        /**
         * \brief Get an atlas contains glyphs bitmap.
         * 
         * \param handle        Handle to the atlas get context returned in begin_get_atlas.
         * \param idx           Index of the typeface we want to get glyph bitmaps from.
         * \param start_code    First unicode point in a range to get glyph bitmap. 0 to use unicode array.
         * \param unicode_point Pointer to an array of unicode point which we want to rasterize. NULL to ingore.
         * \param num_code      Number of unicode point to rasterize.
         * \param font_size     Size of the font to render.
         * \param info          Pointer to array which will contains character info in the atlas. Can be NULL to ignore.
         * 
         * \returns True on success. 
         */
        virtual bool get_glyph_atlas(const std::int32_t handle, const std::size_t idx, const char16_t start_code, int *unicode_point,
            const char16_t num_code, const int font_size, character_info *info)
            = 0;

        // End getting atlas.
        virtual void end_get_atlas(const std::int32_t handle) = 0;

        /**
         * \brief Get total number of font this file consists of.
         * \returns Number of font in this file.
         */
        virtual std::size_t count() = 0;

        /**
         * @brief       Get the unique ID of a typeface.
         * @param       face_index        The index of the face to get the UID from.
         * @returns     0xFFFFFFFF on failure or unavailable
         */
        virtual std::uint32_t unique_id(const std::size_t face_index) = 0;
    };

    enum class font_file_adapter_kind {
        none,
        stb,
        gdr
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
