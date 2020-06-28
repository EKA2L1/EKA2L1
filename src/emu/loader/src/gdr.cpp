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

    bool parse_typeface_list_headers(common::ro_stream *stream, std::vector<typeface_header> &headers) {
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

        return true;
    }

    bool parse_font_bitmap_character_metric(common::ro_stream *stream, character_metric &metric) {
        std::size_t read = 0;
        read += stream->read(&metric.ascent_in_pixels_, sizeof(metric.ascent_in_pixels_));
        read += stream->read(&metric.height_in_pixels_, sizeof(metric.height_in_pixels_));
        read += stream->read(&metric.left_adj_in_pixels_, sizeof(metric.left_adj_in_pixels_));
        read += stream->read(&metric.move_in_pixels_, sizeof(metric.move_in_pixels_));
        read += stream->read(&metric.right_adjust_in_pixels_, sizeof(metric.right_adjust_in_pixels_));

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

    bool parse_font_code_section_comps(common::ro_stream *stream, std::vector<character_metric> &metrics, code_section &section) {
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
                section.chars_.push_back(std::move(c));
                continue;
            }

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

            section.chars_.push_back(std::move(c));
        }

        return true;
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

        if (!parse_typeface_list_headers(stream, typeface_headers)) {
            return false;
        }

        // Assign typeface headers to typeface
        store.typefaces_.resize(typeface_headers.size());
        for (std::size_t i = 0; i < typeface_headers.size(); i++) {
            store.typefaces_[i].header_ = std::move(typeface_headers[i]);
        }

        // Assign font bitmap header to font bitmap and read its components
        store.font_bitmaps_.resize(font_bitmap_headers.size());

        for (std::size_t i = 0; i < font_bitmap_headers.size(); i++) {
            store.font_bitmaps_[i].header_ = font_bitmap_headers[i];

            if (!parse_font_bitmap_character_metrics(stream, store.font_bitmaps_[i].metrics_)) {
                return false;
            }

            store.font_bitmaps_[i].code_sections_.resize(font_bitmap_headers[i].code_section_count_);

            for (std::size_t j = 0; j < font_bitmap_headers[i].code_section_count_; j++) {
                store.font_bitmaps_[i].code_sections_[j].header_ = std::move(code_section_header_list_list[i][j]);

                if (!parse_font_code_section_comps(stream, store.font_bitmaps_[i].metrics_, store.font_bitmaps_[i].code_sections_[j])) {
                    return false;
                }
            }
        }

        return true;
    }
}