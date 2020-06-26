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
        read += stream->read(&the_header.components_offset_, sizeof(the_header.components_offset_));

        static constexpr std::size_t size_of_beginning_header = sizeof(the_header.store_write_once_layout_uid_) + sizeof(the_header.font_store_file_uid_)
            + sizeof(the_header.null_uid_) + sizeof(the_header.font_store_file_checksum_) + sizeof(the_header.id_offset_) + sizeof(the_header.fnt_tran_version_)
            + sizeof(the_header.collection_uid_) + sizeof(the_header.pixel_aspect_ratio_) + sizeof(the_header.components_offset_);

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

    bool parse_store(common::ro_stream *stream, file_store &store) {
        if (!parse_store_header(stream, store.header_)) {
            return false;
        }

        std::vector<font_bitmap_header> font_bitmap_headers;
        std::vector<code_section_header_list> code_section_header_list_list;

        if (!parse_font_bitmap_headers(stream, font_bitmap_headers, code_section_header_list_list)) {
            return false;
        }

        return true;
    }
}