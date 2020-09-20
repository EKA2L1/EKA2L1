/*
 * Copyright (c) 2020 EKA2L1 Team
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

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace eka2l1::common {
    class ro_stream;
}

namespace eka2l1::loader::gdr {
    struct header {
        std::uint32_t store_write_once_layout_uid_;
        std::uint32_t font_store_file_uid_;
        std::uint32_t null_uid_;
        std::uint32_t font_store_file_checksum_;
        std::uint32_t id_offset_;
        std::uint32_t fnt_tran_version_;
        std::uint32_t collection_uid_;
        std::uint32_t pixel_aspect_ratio_;
        std::uint32_t id_offset_2_;
        std::vector<std::u16string> copyright_strings_;
    };

    struct code_section_header {
        std::uint16_t start_;           ///< The starting character code of this section.
        std::uint16_t end_;             ///< The last character code of this section
        std::uint32_t character_offset_;
        std::uint32_t character_bitmap_offset_;
    };

    #pragma pack(push, 1)
    struct font_bitmap_header {
        std::uint32_t uid_;
        std::uint8_t posture_;
        std::uint8_t stroke_weight_;
        std::uint8_t is_proportional_;
        std::uint8_t cell_height_in_pixels_;
        std::uint8_t ascent_in_pixels_;
        std::uint8_t max_char_width_in_pixels_;
        std::uint8_t max_normal_char_width_in_pixels_;
        std::uint32_t bitmap_encoding_;
        std::uint32_t metric_offset_;
        std::uint32_t metric_count_;
        std::uint32_t code_section_count_;
    };
    #pragma pack(pop)

    #pragma pack(push, 1)
    struct typeface_font_bitmap_header {
        std::uint32_t uid_;
        std::uint8_t width_factor_;
        std::uint8_t height_factor_;
    };
    #pragma pack(pop)

    struct typeface_header {
        std::u16string name_;
        std::uint8_t flags_;
        std::vector<typeface_font_bitmap_header> bitmap_headers_;
    };

    struct character_metric {
        std::uint8_t ascent_in_pixels_;
        std::uint8_t height_in_pixels_;
        std::uint8_t left_adj_in_pixels_;
        std::uint8_t move_in_pixels_;
        std::uint8_t right_adjust_in_pixels_;
    };

    using bitmap = std::vector<std::uint32_t>;

    struct character {
        character_metric *metric_;
        bitmap data_;
    };

    struct code_section {
        code_section_header header_;
        std::vector<character> chars_;
    };

    struct font_bitmap {
        font_bitmap_header header_;
        std::vector<character_metric> metrics_;
        std::vector<code_section> code_sections_;

        character_metric filler_metric_;

        // This enum value based on TrueType's
        enum {
            COVERAGE_LATIN_SET = 0x1,
            COVERAGE_GREEK_SET = 0x80,
            COVERAGE_CYRILLIC_SET = 0x200,
            COVERAGE_ARMENIAN_SET = 0x400,
            COVERAGE_HEBREW_SET = 0x800,
            COVERAGE_ARABIC_SET = 0x2000,
            COVERAGE_DEVANAGARI_SET = 0x8000,
            COVERAGE_BENGALI_SET = 0x10000,
            COVERAGE_GURMUKHI_SET = 0x20000,
            COVERAGE_GUJURATI_SET = 0x40000,
            COVERAGE_ORIYA_SET = 0x80000,
            COVERAGE_TAMIL_SET = 0x100000,
            COVERAGE_TELUGU_SET = 0x200000,
            COVERAGE_KANNADA_SET = 0x400000,
            COVERAGE_MALAYALAM_SET = 0x800000,
            COVERAGE_THAI_SET = 0x1000000,
            COVERAGE_LAO_SET = 0x2000000,
            COVERAGE_GEORGIAN_SET = 0x8000000,
            COVERAGE_HANGUL_JAMO_SET = 0x10000000,
            COVERAGE_SYMBOL_SETS = 0xFFFE,
            COVERAGE_KANA_SETS = 0x60000,
            COVERAGE_HANGUL_SET = 0x100000,
            COVERAGE_CJK_SET = 0x8000000
        };

        std::uint32_t coverage_[4];
    };

    struct typeface {
        enum {
            FLAG_ITALIC = 1 << 0,
            FLAG_BOLD = 1 << 1
        };

        typeface_header header_;

        std::vector<font_bitmap*> font_bitmaps_;
        std::uint32_t whole_coverage_[4];
        std::uint32_t analysed_style_;
    };

    struct file_store {
        header header_;

        std::vector<font_bitmap> font_bitmaps_;
        std::vector<typeface> typefaces_;

        std::u16string original_typeface_guess_name_;
    };

    /**
     * @brief   Parse a GDR structure from a stream.
     * 
     * @param   stream  Pointer to the stream.
     * @param   store   Reference to the file store to be filled.
     * 
     * @returns True on success.
     */
    bool parse_store(common::ro_stream *stream, file_store &store);
}