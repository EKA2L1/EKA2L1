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

#include <services/fbs/adapter/font_adapter.h>
#include <services/fbs/adapter/gdr_font_adapter.h>
#include <services/fbs/adapter/stb_font_adapter.h>

namespace eka2l1::epoc::adapter {
    bool font_file_adapter_base::make_text_shape(const std::size_t face_index, const open_font_shaping_parameter &params, const std::u16string &text, const std::uint32_t metric_identifier, open_font_shaping_header &shaping_header, std::uint8_t *shaping_data) {
        if (params.text_range_[0] > params.text_range_[1]) {
            LOG_ERROR(SERVICE_FBS, "Text start position is larger than text end position in shaping parameter!");
            return false;
        }

        const std::size_t start_pos = common::max<std::size_t>(static_cast<std::size_t>(params.text_range_[0]), text.length() - 1);
        const std::size_t end_pos = common::max<std::size_t>(static_cast<std::size_t>(params.text_range_[1]), text.length() - 1);

        shaping_header.char_count_ = end_pos - start_pos;
        shaping_header.glyph_count_ = 0;

        for (std::size_t i = start_pos; i < end_pos; i++) {
            // Depends on implementation, but for now gonna copy paste all here
            if ((text[i] >= 0x200c && text[i] <= 0x200f) || (text[i] >= 0x202a && text[i] <= 0x202e) || (text[i] >= 0xfffe && text[i] <= 0xffff)) {
                // Skip control characters
                // TODO: Handle them properly
                continue;
            }

            shaping_header.glyph_count_++;
        }

        if (!shaping_data) {
            return true;
        }

        std::uint32_t *glyph_code = reinterpret_cast<std::uint32_t*>(shaping_data);
        std::uint16_t *positions_and_advance = reinterpret_cast<std::uint16_t*>(shaping_data + shaping_header.glyph_count_ * 4);
        std::uint16_t *glyph_index_in_text = reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uint8_t*>(positions_and_advance) + shaping_header.glyph_count_ * 4 + 4);

        eka2l1::vec2 current_position = eka2l1::vec2(0, 0);

        // Supply shaping data
        for (std::size_t i = start_pos, current_glyph_index = 0; i < end_pos; i++) {
            // Depends on implementation, but for now gonna copy paste all here
            if ((text[i] >= 0x200c && text[i] <= 0x200f) || (text[i] >= 0x202a && text[i] <= 0x202e) || (text[i] >= 0xfffe && text[i] <= 0xffff)) {
                // Skip control characters
                // TODO: Handle them properly
                continue;
            }

            // TODO: Account parameters to advance text vertically maybe! For now it's all horizontal :(
            glyph_code[current_glyph_index] = text[i];
            positions_and_advance[current_glyph_index * 2] = static_cast<std::uint16_t>(current_position.x);
            positions_and_advance[current_glyph_index * 2 + 1] = static_cast<std::uint16_t>(current_position.y);
            glyph_index_in_text[current_glyph_index] = static_cast<std::uint16_t>(i);

            const std::uint32_t adv = get_glyph_advance(face_index, text[i], metric_identifier, false);
            if (adv == 0xFFFFFFFF) {
                current_position.x += 1;
            } else {
                current_position.x += adv;
            }

            current_glyph_index++;
        }

        positions_and_advance[shaping_header.glyph_count_ * 2] = static_cast<std::uint16_t>(current_position.x);
        positions_and_advance[shaping_header.glyph_count_ * 2 + 1] = static_cast<std::uint16_t>(current_position.y);

        return true;
    }

    std::unique_ptr<font_file_adapter_base> make_font_file_adapter(const font_file_adapter_kind kind, std::vector<std::uint8_t> &dat) {
        switch (kind) {
        case font_file_adapter_kind::stb: {
            return std::make_unique<stb_font_file_adapter>(dat);
        }

        case font_file_adapter_kind::gdr: {
            return std::make_unique<gdr_font_file_adapter>(dat);
        }

        default: {
            break;
        }
        }

        return nullptr;
    }
}
