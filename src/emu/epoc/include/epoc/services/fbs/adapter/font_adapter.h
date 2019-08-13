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

#include <epoc/services/fbs/font.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace eka2l1::epoc::adapter {
    /**
     * \brief Base class for adapter.
     */
    class font_file_adapter_base {
    public:
        virtual bool get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) = 0;
        virtual bool get_metrics(const std::size_t idx, open_font_metrics &metrics) = 0;
        virtual bool get_glyph_metric(const std::size_t idx, std::uint32_t code, open_font_character_metric &metric,
             const std::int32_t baseline_horz_off, const float scale_x = 1.0f, const float scale_y = 1.0f) = 0;

        virtual std::uint8_t *get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const float scale_x,
            const float scale_y, int *rasterized_width, int *rasterized_height, epoc::glyph_bitmap_type *bmp_type) = 0;

        virtual void free_glyph_bitmap(std::uint8_t *data) = 0;
        virtual glyph_bitmap_type get_output_bitmap_type() const = 0;
        
        /**
         * \brief Get total number of font this file consists of.
         * \returns Number of font in this file.
         */
        virtual std::size_t count() = 0;
    };

    enum class font_file_adapter_kind {
        stb
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
