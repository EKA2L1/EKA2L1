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

#include <common/buffer.h>
#include <loader/gdr.h>
#include <utils/des.h>

namespace eka2l1::loader::gdr {
    static void scan_coverage(const code_section &range, std::uint32_t &coverage_flags, std::uint32_t &coverage_flags_add) {
        // 0000-007F
        if (range.header_.start_ >= 0x00) {
            coverage_flags |= font_bitmap::COVERAGE_LATIN_SET;
        }

        // 0370-03FF
        if (range.header_.end_ >= 0x370) {
            coverage_flags |= font_bitmap::COVERAGE_GREEK_SET;
        }

        // 0400-04FF
        if (range.header_.end_ >= 0x400) {
            coverage_flags |= font_bitmap::COVERAGE_CYRILLIC_SET;
        }

        // 0530-058F
        if (range.header_.end_ >= 0x530) {
            coverage_flags |= font_bitmap::COVERAGE_ARMENIAN_SET;
        }

        // 0590-05FF
        if (range.header_.end_ >= 0x590) {
            coverage_flags |= font_bitmap::COVERAGE_HEBREW_SET;
        }

        // 0600-06FF
        if (range.header_.end_ >= 0x600) {
            coverage_flags |= font_bitmap::COVERAGE_ARABIC_SET;
        }

        // 0900-097F
        if (range.header_.end_ >= 0x900) {
            coverage_flags |= font_bitmap::COVERAGE_DEVANAGARI_SET;
        }

        // 0980-09FF
        if (range.header_.end_ >= 0x980) {
            coverage_flags |= font_bitmap::COVERAGE_BENGALI_SET;
        }

        // 0A00-0A7F
        if (range.header_.end_ >= 0xA00) {
            coverage_flags |= font_bitmap::COVERAGE_GURMUKHI_SET;
        }

        // 0A80-0AFF
        if (range.header_.end_ >= 0xA80) {
            coverage_flags |= font_bitmap::COVERAGE_GUJURATI_SET;
        }

        // 0B00-0B7F
        if (range.header_.end_ >= 0xB00) {
            coverage_flags |= font_bitmap::COVERAGE_ORIYA_SET;
        }

        // 0B80-0BFF
        if (range.header_.end_ >= 0xB80) {
            coverage_flags |= font_bitmap::COVERAGE_TAMIL_SET;
        }

        // 0C00-0C7F
        if (range.header_.end_ >= 0xC00) {
            coverage_flags |= font_bitmap::COVERAGE_TELUGU_SET;
        }

        // 0C80-0CFF
        if (range.header_.end_ >= 0xC80) {
            coverage_flags |= font_bitmap::COVERAGE_KANNADA_SET;
        }

        // 0D00-0D7F
        if (range.header_.end_ >= 0xD00) {
            coverage_flags |= font_bitmap::COVERAGE_MALAYALAM_SET;
        }

        //  0E00-0E7F
        if (range.header_.end_ >= 0xE00) {
            coverage_flags |= font_bitmap::COVERAGE_THAI_SET;
        }

        // 0E80-0EFF
        if (range.header_.end_ >= 0xE80) {
            coverage_flags |= font_bitmap::COVERAGE_LAO_SET;
        }

        // 10A0-10FF
        if (range.header_.end_ >= 0x10A0) {
            coverage_flags |= font_bitmap::COVERAGE_GEORGIAN_SET;
        }

        // 1100-11FF
        if (range.header_.end_ >= 0x1100) {
            coverage_flags |= font_bitmap::COVERAGE_HANGUL_JAMO_SET;
        }

        // NOTICE: I got lazy from here.

        // 30A0 - 30FF
        if (range.header_.end_ >= 0x30A0) {
            coverage_flags_add |= font_bitmap::COVERAGE_KANA_SETS;
        }

        // Hangul
        // U+AC00–U+D7AF, U+1100–U+11FF, U+3130–U+318F, U+A960–U+A97F, U+D7B0–U+D7FF
        if ((range.header_.end_ >= 0xAC00) || (range.header_.end_ >= 0x1100) || (range.header_.end_ >= 0x3130) || (range.header_.end_ >= 0xA960)
            || (range.header_.end_ >= 0xD7B0)) {
            coverage_flags_add |= font_bitmap::COVERAGE_HANGUL_SET;
        }
    }

    bool parse_store_header(common::ro_stream *stream, header &the_header) {
        std::size_t read = 0;

        read += stream->read(&the_header.store_write_once_layout_uid_, sizeof(the_header.store_write_once_layout_uid_));
        read += stream->read(&the_header.font_store_file_uid_, sizeof(the_header.font_store_file_uid_));
        read += stream->read(&the_header.null_uid_, sizeof(the_header.null_uid_));
        read += stream->read(&the_header.font_store_file_checksum_, sizeof(the_header.font_store_file_checksum_));
        read += stream->read(&the_header.id_offset_, sizeof(the_header.id_offset_));
        read += stream->read(&the_header.fnt_tran_version_, sizeof(the_header.fnt_tran_version_));
        read += stream->read(&the_header.collection_uid_, sizeof(the_header.collection_uid_));
        read += stream->read(&the_header.pixel_aspect_ratio_, sizeof(the_header.pixel_aspect_ratio_));
        read += stream->read(&the_header.id_offset_2_, sizeof(the_header.id_offset_2_));

        static constexpr std::size_t size_of_beginning_header = sizeof(the_header.store_write_once_layout_uid_) + sizeof(the_header.font_store_file_uid_)
            + sizeof(the_header.null_uid_) + sizeof(the_header.font_store_file_checksum_) + sizeof(the_header.id_offset_) + sizeof(the_header.fnt_tran_version_)
            + sizeof(the_header.collection_uid_) + sizeof(the_header.pixel_aspect_ratio_) + sizeof(the_header.id_offset_2_);

        if (read != size_of_beginning_header) {
            return false;
        }
        
        std::uint32_t copyright_info_string_count = 0;
        if (stream->read(&copyright_info_string_count, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            return false;
        }

        for (std::uint32_t i = 0; i < copyright_info_string_count; i++) {
            std::u16string copyright_info;
            if (!epoc::read_des_string(copyright_info, stream, true)) {
                return false;
            }

            the_header.copyright_strings_.push_back(copyright_info);
        }

        return true;
    }

    using code_section_header_list = std::vector<code_section_header>;

    bool parse_code_section_header(common::ro_stream *stream, code_section_header &header) {
        std::size_t read = 0;
        read += stream->read(&header.start_, sizeof(header.start_));
        read += stream->read(&header.end_, sizeof(header.end_));
        read += stream->read(&header.character_offset_, sizeof(header.character_offset_));
        read += stream->read(&header.character_bitmap_offset_, sizeof(header.character_bitmap_offset_));

        return (read == sizeof(code_section_header));
    }

    bool parse_font_bitmap_header(common::ro_stream *stream, font_bitmap_header &header, code_section_header_list &section_header_list) {
        std::size_t read = 0;
        read += stream->read(&header.uid_, sizeof(header.uid_));
        read += stream->read(&header.posture_, sizeof(header.posture_));
        read += stream->read(&header.stroke_weight_, sizeof(header.stroke_weight_));
        read += stream->read(&header.is_proportional_, sizeof(header.is_proportional_));
        read += stream->read(&header.cell_height_in_pixels_, sizeof(header.cell_height_in_pixels_));
        read += stream->read(&header.ascent_in_pixels_, sizeof(header.ascent_in_pixels_));
        read += stream->read(&header.max_char_width_in_pixels_, sizeof(header.max_char_width_in_pixels_));
        read += stream->read(&header.max_normal_char_width_in_pixels_, sizeof(header.max_normal_char_width_in_pixels_));
        read += stream->read(&header.bitmap_encoding_, sizeof(header.bitmap_encoding_));
        read += stream->read(&header.metric_offset_, sizeof(header.metric_offset_));
        read += stream->read(&header.metric_count_, sizeof(header.metric_count_));
        read += stream->read(&header.code_section_count_, sizeof(header.code_section_count_));

        if (read != sizeof(font_bitmap_header)) {
            return false;
        }

        // Read code section header
        for (std::uint32_t i = 0; i < header.code_section_count_; i++) {
            code_section_header section_header;

            if (!parse_code_section_header(stream, section_header)) {
                return false;
            }

            section_header_list.push_back(section_header);
        }

        return true;
    }

    bool parse_font_bitmap_headers(common::ro_stream *stream, std::vector<font_bitmap_header> &headers, std::vector<code_section_header_list> &code_sections) {
        std::uint32_t count = 0;
        if (stream->read(&count, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            return false;
        }

        for (std::uint32_t i = 0; i < count; i++) {
            font_bitmap_header new_header;
            code_section_header_list section_header_list;

            if (!parse_font_bitmap_header(stream, new_header, section_header_list)) {
                return false;
            }

            headers.push_back(std::move(new_header));
            code_sections.push_back(std::move(section_header_list));
        }

        return true;
    }

    bool parse_typeface_font_bitmap_header(common::ro_stream *stream, typeface_font_bitmap_header &header) {
        std::size_t read = 0;
        read += stream->read(&header.uid_, sizeof(header.uid_));
        read += stream->read(&header.width_factor_, sizeof(header.width_factor_));
        read += stream->read(&header.height_factor_, sizeof(header.height_factor_));

        return (read == sizeof(typeface_font_bitmap_header));
    }

    bool parse_typeface_list_header(common::ro_stream *stream, typeface_header &header) {
        if (!epoc::read_des_string(header.name_, stream, true)) {
            return false;
        }

        if (stream->read(&header.flags_, sizeof(header.flags_)) != sizeof(header.flags_)) {
            return false;
        }

        std::uint32_t num_typeface_font_bitmap_header = 0;
        if (stream->read(&num_typeface_font_bitmap_header, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            return false;
        }

        header.bitmap_headers_.resize(num_typeface_font_bitmap_header);

        for (std::size_t i = 0; i < header.bitmap_headers_.size(); i++) {
            if (!parse_typeface_font_bitmap_header(stream, header.bitmap_headers_[i])) {
                return false;
            }
        }

        return true;
    }

    bool parse_typeface_list_headers(common::ro_stream *stream, std::vector<typeface_header> &headers,
        std::u16string &original_guess_typeface_name) {
        std::uint32_t header_count = 0;
        if (stream->read(&header_count, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            return false;
        }

        headers.resize(header_count);

        for (std::size_t i = 0; i < header_count; i++) {
            if (!parse_typeface_list_header(stream, headers[i])) {
                return false;
            }
        }

        original_guess_typeface_name.clear();
        
        // Try to guess original typeface name.
        if (headers.size() > 1) {
            std::size_t length_overlap = 1;

            for (length_overlap = 1; length_overlap < headers[0].name_.size() - 1; length_overlap++) {
                const std::u16string part = headers[0].name_.substr(0, length_overlap);
                bool found = true;

                for (std::size_t i = 1; i < headers.size(); i++) {
                    const std::u16string another_part = headers[i].name_.substr(0, length_overlap);
                    if (common::compare_ignore_case(part, another_part) != 0) {
                        found = false;
                        break;
                    }
                }

                if (!found) {
                    break;
                } else {
                    original_guess_typeface_name = part;
                }
            }
        }

        return true;
    }

    bool parse_font_bitmap_character_metric(common::ro_stream *stream, character_metric &metric) {
        std::size_t read = 0;
        read += stream->read(&metric.ascent_in_pixels_, sizeof(metric.ascent_in_pixels_));
        read += stream->read(&metric.height_in_pixels_, sizeof(metric.height_in_pixels_));
        read += stream->read(&metric.left_adj_in_pixels_, sizeof(metric.left_adj_in_pixels_));
        read += stream->read(&metric.move_in_pixels_, sizeof(metric.move_in_pixels_));
        read += stream->read(&metric.right_adjust_in_pixels_, sizeof(metric.right_adjust_in_pixels_));

        // Im scared of this being a hack.
        if (metric.right_adjust_in_pixels_ == 0xFF) {
            metric.right_adjust_in_pixels_ = 0;
        }

        return (read == sizeof(character_metric));
    }

    bool parse_font_bitmap_character_metrics(common::ro_stream *stream, std::vector<character_metric> &metrics) {
        std::uint32_t metric_count = 0;
        if (stream->read(&metric_count, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            return false;
        }

        metrics.resize(metric_count);

        for (std::size_t i = 0; i < metrics.size(); i++) {
            if (!parse_font_bitmap_character_metric(stream, metrics[i])) {
                return false;
            }
        }

        return true;
    }

    bool parse_font_code_section_comps(common::ro_stream *stream, std::vector<character_metric> &metrics, character_metric &filler_metric, code_section &section) {
        std::vector<std::uint16_t> offsets;
        std::uint32_t num_offset = 0;

        if (stream->read(&num_offset, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            return false;
        }

        offsets.resize(num_offset);

        for (std::uint32_t i = 0; i < num_offset; i++) {
            if (stream->read(&offsets[i], sizeof(std::uint16_t)) != sizeof(std::uint16_t)) {
                return false;
            }
        }

        std::uint32_t len_byte_list = 0;
        if (stream->read(&len_byte_list, sizeof(std::uint32_t)) != sizeof(std::uint32_t)) {
            return false;
        }

        std::vector<std::uint8_t> byte_list;
        byte_list.resize(len_byte_list);

        if (stream->read(&byte_list[0], len_byte_list) != len_byte_list) {
            return false;
        }

        // Parse this byte list
        for (std::uint16_t i = section.header_.start_; i <= section.header_.end_; i++) {
            character c;
            c.metric_ = nullptr;

            if (offsets[i - section.header_.start_] >= 0x7FFF) {
                // This character is filler
                c.metric_ = &filler_metric;
                
                // Give it an empty character data.
                c.data_.resize((filler_metric.move_in_pixels_ * filler_metric.height_in_pixels_ + 31) >> 5);
                std::fill(c.data_.begin(), c.data_.end(), 0);
            } else {
                std::uint8_t *byte_start = byte_list.data() + offsets[i - section.header_.start_];
                std::uint32_t bitoffset = 0;
                std::uint16_t metric_index = 0;
                std::uint8_t total_bit_for_index = 0;

                #define GET_BIT_8(offset) ((byte_start[offset >> 3] >> (offset & 7)) & 1)
                if (GET_BIT_8(bitoffset) == 0) {
                    total_bit_for_index = 7;
                } else {
                    total_bit_for_index = 15;
                }

                bitoffset++;

                for (std::uint8_t idx = 0; idx < total_bit_for_index; idx++) {
                    metric_index |= GET_BIT_8(bitoffset) << idx;
                    bitoffset++;
                }

                c.metric_ = &metrics[metric_index];
                
                const std::uint16_t target_height = c.metric_->height_in_pixels_;
                const std::uint16_t content_width = c.metric_->move_in_pixels_ - c.metric_->left_adj_in_pixels_ - 
                    ((c.metric_->right_adjust_in_pixels_ == 0xFF) ? 0 : c.metric_->right_adjust_in_pixels_);
                std::uint16_t height_read = 0;
            
                c.data_.resize((target_height * content_width + 31) >> 5);
                std::fill(c.data_.begin(), c.data_.end(), 0);
        
                while (height_read < target_height) {      
                    bool repeat_line = !GET_BIT_8(bitoffset);
                    bitoffset++;
                    
                    std::uint16_t line_count = 0;
                    for (int i = 0; i < 4; i++) {
                        line_count |= GET_BIT_8(bitoffset) << i;
                        bitoffset++;
                    }
                    
                    for (std::uint16_t y = 0; y < (repeat_line ? 1 : line_count); y++) {          
                        for (std::uint16_t x = 0; x < content_width; x++) {
                            std::uint32_t current_pixel = ((y + height_read) * content_width + x);
                            std::uint8_t the_bit = GET_BIT_8(bitoffset);

                            if (repeat_line) {
                                for (std::size_t k = 0; k < line_count; k++) {
                                    c.data_[current_pixel >> 5] |= the_bit << (current_pixel & 31);
                                    current_pixel += content_width;
                                }
                            } else {
                                c.data_[current_pixel >> 5] |= the_bit << (current_pixel & 31);
                            }
                                        
                            bitoffset++;
                        }
                    }
                    
                    // Align to next byte
                    height_read += line_count;
                }

                #undef GET_BIT_8
            }

            section.chars_.push_back(std::move(c));
        }

        return true;
    }

    static void get_fill_character_metric(font_bitmap_header &bitmap_header, character_metric &fill_metric) {
        fill_metric.ascent_in_pixels_ = bitmap_header.ascent_in_pixels_;
        fill_metric.height_in_pixels_ = bitmap_header.cell_height_in_pixels_;
        fill_metric.left_adj_in_pixels_ = 0;
        fill_metric.right_adjust_in_pixels_ = 0;
        fill_metric.move_in_pixels_ = bitmap_header.max_normal_char_width_in_pixels_;
    }

    bool parse_store(common::ro_stream *stream, file_store &store) {
        if (!parse_store_header(stream, store.header_)) {
            return false;
        }

        std::vector<font_bitmap_header> font_bitmap_headers;
        std::vector<code_section_header_list> code_section_header_list_list;
        std::vector<typeface_header> typeface_headers;

        if (!parse_font_bitmap_headers(stream, font_bitmap_headers, code_section_header_list_list)) {
            return false;
        }

        if (!parse_typeface_list_headers(stream, typeface_headers, store.original_typeface_guess_name_)) {
            return false;
        }

        // Assign font bitmap header to font bitmap and read its components
        store.font_bitmaps_.resize(font_bitmap_headers.size());

        for (std::size_t i = 0; i < font_bitmap_headers.size(); i++) {
            store.font_bitmaps_[i].header_ = font_bitmap_headers[i];
            get_fill_character_metric(store.font_bitmaps_[i].header_, store.font_bitmaps_[i].filler_metric_);

            std::fill(store.font_bitmaps_[i].coverage_, store.font_bitmaps_[i].coverage_ + 4, 0);

            if (!parse_font_bitmap_character_metrics(stream, store.font_bitmaps_[i].metrics_)) {
                return false;
            }

            store.font_bitmaps_[i].code_sections_.resize(font_bitmap_headers[i].code_section_count_);

            for (std::size_t j = 0; j < font_bitmap_headers[i].code_section_count_; j++) {
                store.font_bitmaps_[i].code_sections_[j].header_ = std::move(code_section_header_list_list[i][j]);

                if (!parse_font_code_section_comps(stream, store.font_bitmaps_[i].metrics_, store.font_bitmaps_[i].filler_metric_,
                    store.font_bitmaps_[i].code_sections_[j])) {
                    return false;
                }

                scan_coverage(store.font_bitmaps_[i].code_sections_[j], store.font_bitmaps_[i].coverage_[0], store.font_bitmaps_[i].coverage_[1]);
            }
        }
        
        // Sort all font bitmaps by UID
        std::sort(store.font_bitmaps_.begin(), store.font_bitmaps_.end(), [](const loader::gdr::font_bitmap &f1, const loader::gdr::font_bitmap &f2) {
            return f1.header_.uid_ < f2.header_.uid_;
        });

        // Assign typeface headers to typeface
        store.typefaces_.resize(typeface_headers.size());
        for (std::size_t i = 0; i < typeface_headers.size(); i++) {
            store.typefaces_[i].header_ = std::move(typeface_headers[i]);
            std::fill(store.typefaces_[i].whole_coverage_, store.typefaces_[i].whole_coverage_ + 4, 0);

            store.typefaces_[i].analysed_style_ = 0;

            // Find all font bitmaps
            for (auto &bitmap_header: store.typefaces_[i].header_.bitmap_headers_) {
                auto result = std::lower_bound(store.font_bitmaps_.begin(), store.font_bitmaps_.end(), bitmap_header.uid_, 
                    [](const loader::gdr::font_bitmap &b, const std::uint32_t uid) {
                        return b.header_.uid_ < uid;
                    });

                if ((result == store.font_bitmaps_.end()) || (result->header_.uid_ != bitmap_header.uid_)) {
                    return false;
                }

                if (result->header_.stroke_weight_) {
                    store.typefaces_[i].analysed_style_ |= typeface::FLAG_BOLD;
                }

                if (result->header_.posture_) {
                    store.typefaces_[i].analysed_style_ |= typeface::FLAG_ITALIC;
                }

                store.typefaces_[i].font_bitmaps_.push_back(&(*result));
                
                for (std::uint32_t j = 0; j < 4; j++) {
                    store.typefaces_[i].whole_coverage_[j] |= result->coverage_[j];
                }
            }
            
            if (!store.original_typeface_guess_name_.empty()) {
                // Extract style flags name
                std::u16string org_name = store.typefaces_[i].header_.name_;
                const std::size_t first_numeric = org_name.find_first_of(u"0123456789");

                if (first_numeric != std::u16string::npos) {
                    org_name = common::lowercase_ucs2_string(org_name.substr(store.original_typeface_guess_name_.length(),
                        first_numeric - store.original_typeface_guess_name_.length()));

                    static const std::map<std::u16string, std::uint32_t> FLAG_SEARCHS = {
                        { u"bold", typeface::FLAG_BOLD },
                        { u"pb", typeface::FLAG_BOLD },
                        { u"b", typeface::FLAG_BOLD },
                        { u"italic", typeface::FLAG_ITALIC },
                        { u"pi", typeface::FLAG_ITALIC },
                        { u"bi", typeface::FLAG_BOLD | typeface::FLAG_ITALIC },
                        { u"plain", 0 },
                        { u"p", 0 }
                    };

                    for (auto &flag_search: FLAG_SEARCHS) {
                        if (org_name == flag_search.first) {
                            store.typefaces_[i].analysed_style_ |= flag_search.second;
                            break;
                        }
                    }
                }
            }
        }

        return true;
    }
}