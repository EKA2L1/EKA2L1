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
#include <services/fbs/adapter/freetype_font_adapter.h>
#include <services/fbs/adapter/gdr_font_adapter.h>
#include <services/fbs/adapter/stb_font_adapter.h>
#include <common/bytes.h>
#include <common/log.h>

namespace eka2l1::epoc::adapter {

    std::size_t monochrome_glyph_word_count(const std::int32_t width, const std::int32_t height) {
        // Enough for the scanline bits even when nothing compresses, plus room
        // for the run headers.
        return ((static_cast<std::uint32_t>(width * height) + 31) >> 5) + 5;
    }

    std::uint32_t compress_monochrome_glyph(const std::uint32_t *src, const std::int32_t width,
        const std::int32_t height, std::uint32_t *dest) {
        std::int32_t total_line_processed_so_far = 0;
        std::uint32_t total_bit_write = 0;

#define WRITE_BIT_32(bit)                                                               \
    dest[(total_bit_write >> 5)] |= ((bit & 1) << (total_bit_write & 31)); \
    total_bit_write++

        auto compare_line_equal = [&](std::uint32_t p_l1, std::uint32_t p_l2, const std::uint32_t n) -> bool {
            std::uint32_t left = n;

            while (left > 0) {
                std::uint32_t to_read = std::min<std::uint32_t>(left, 32);

                std::uint32_t pos1 = (p_l1 * width + n - left);
                std::uint32_t pos2 = (p_l2 * width + n - left);

                std::uint32_t maximum_1 = 32U - (pos1 & 31);
                std::uint32_t maximum_2 = 32U - (pos2 & 31);

                std::uint32_t part1read = std::min<std::uint32_t>(maximum_1, to_read);
                std::uint32_t part2read = std::min<std::uint32_t>(maximum_2, to_read);

                // common::extract_bits numbers bits from one, so a zero based
                // position has to be handed over as p + 1. Passing zero shifts
                // by an underflowed count, which is how identical scanlines
                // used to compare unequal and every glyph ended up written out
                // in full.
                std::uint32_t l1p = common::extract_bits(src[pos1 >> 5], (pos1 & 31) + 1, part1read) | ((maximum_1 < to_read) ? (common::extract_bits(src[(pos1 >> 5) + 1], 1, to_read - maximum_1) << part1read) : 0);

                std::uint32_t l2p = common::extract_bits(src[pos2 >> 5], (pos2 & 31) + 1, part2read) | ((maximum_2 < to_read) ? (common::extract_bits(src[(pos2 >> 5) + 1], 1, to_read - maximum_2) << part2read) : 0);

                if (l1p != l2p) {
                    return false;
                }

                left -= to_read;
            }

            return true;
        };

        while (total_line_processed_so_far < height) {
            bool mode = false;
            std::int8_t count = 2;

            if (total_line_processed_so_far == (height - 1)) {
                count = 1;
                mode = false;
            } else {
                mode = compare_line_equal(total_line_processed_so_far, total_line_processed_so_far + 1, width);

                bool got_in = false;

                while ((count < 15) && (total_line_processed_so_far + count < height) && (compare_line_equal(total_line_processed_so_far + (mode ? 0 : (count - 1)), total_line_processed_so_far + count, width) == mode)) {
                    count++;
                    got_in = true;
                }

                if (got_in) {
                    count--;
                }
            }

            WRITE_BIT_32(mode ? 0 : 1); // Repeat mode if line equal

            // Write the repeat count
            WRITE_BIT_32(count & 1);
            WRITE_BIT_32((count >> 1) & 1);
            WRITE_BIT_32((count >> 2) & 1);
            WRITE_BIT_32((count >> 3) & 1);

            // Write the line content
            std::uint32_t loc = total_line_processed_so_far * width;

            for (std::size_t j = 0; j < (mode ? 1 : count); j++) {
                for (std::size_t i = 0; i < width; i++) {
                    // Give up being fast lol
                    WRITE_BIT_32((src[(loc + i) >> 5] >> ((loc + i) & 31)) & 1);
                }

                loc += width;
            }

            total_line_processed_so_far += count;
        }
#undef WRITE_BIT_32

        return total_bit_write;
    }

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

        case font_file_adapter_kind::freetype: {
            return std::make_unique<freetype_font_adapter>(dat);
        }

        default: {
            break;
        }
        }

        return nullptr;
    }
}
