/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <common/vecx.h>
#include <drivers/graphics/common.h>
#include <services/fbs/adapter/font_adapter.h>
#include <services/window/common.h>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace eka2l1::drivers {
    class graphics_driver;
    class graphics_command_list_builder;
}

namespace eka2l1::epoc {
#define ESTIMATE_MAX_CHAR_IN_ATLAS_WIDTH 50

    /**
     * \brief Font atlas is a texture contains glyph bitmaps.
     * 
     * The atlas dimension is a square, and height is equals to font_size *
     * estimate_max_char_in_atlas.
     */
    struct font_atlas {
        std::map<char16_t, adapter::character_info> characters_;
        drivers::handle atlas_handle_;
        adapter::font_file_adapter_base *adapter_;
        int size_;

        std::vector<int> last_use_;

        std::pair<char16_t, char16_t> initial_range_;
        std::unique_ptr<std::uint8_t[]> atlas_data_;

        std::size_t typeface_idx_;

    public:
        explicit font_atlas();

        explicit font_atlas(adapter::font_file_adapter_base *adapter, const std::size_t typeface_idx, const char16_t initial_start,
            const char16_t initial_char_count, int font_size);

        void init(adapter::font_file_adapter_base *adapter, const std::size_t typeface_idx, const char16_t initial_start,
            const char16_t initial_char_count, int font_size);

        void free(drivers::graphics_driver *driver);

        int get_atlas_width() const;

        bool draw_text(const std::u16string &text, const eka2l1::rect &box, const epoc::text_alignment alignment, drivers::graphics_driver *driver,
            drivers::graphics_command_list_builder *builder);
    };
}