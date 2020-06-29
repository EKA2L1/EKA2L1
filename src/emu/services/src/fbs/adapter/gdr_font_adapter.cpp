/*
 * Copyright (c) 2020 EKA2L1 Team.
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

#include <services/fbs/adapter/gdr_font_adapter.h>
#include <common/bytes.h>

namespace eka2l1::epoc::adapter {
    gdr_font_file_adapter::gdr_font_file_adapter(std::vector<std::uint8_t> &data) {
        // Instantiate a read-only buffer stream
        buf_stream_ = std::make_unique<common::ro_buf_stream>(&data[0], data.size());
        
        if (!loader::gdr::parse_store(reinterpret_cast<common::ro_stream*>(buf_stream_.get()), store_)) {
            // Do this so a sanity check can happens
            buf_stream_.release();
        }
    }

    std::size_t gdr_font_file_adapter::count() {
        return store_.typefaces_.size();
    }

    bool gdr_font_file_adapter::get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) {
        // Look for the index of the typeface
        if (!is_valid() || (idx >= store_.typefaces_.size())) {
            return false;
        }

        loader::gdr::typeface &the_typeface = store_.typefaces_[idx];

        // GDR only gives us name. For now assign all
        face_attrib.name.assign(nullptr, the_typeface.header_.name_);
        face_attrib.fam_name.assign(nullptr, the_typeface.header_.name_);
        face_attrib.local_full_name.assign(nullptr, the_typeface.header_.name_);
        face_attrib.local_full_fam_name.assign(nullptr, the_typeface.header_.name_);

        if (the_typeface.header_.flags_ & 0b01) {
            face_attrib.style |= open_font_face_attrib::bold;
        }

        if (the_typeface.header_.flags_ & 0b10) {
            face_attrib.style |= open_font_face_attrib::italic;
        }

        if (the_typeface.header_.flags_ & 0b100) {
            face_attrib.style |= open_font_face_attrib::serif;
        }

        std::memcpy(face_attrib.coverage, the_typeface.whole_coverage_, sizeof(face_attrib.coverage));
        return true;
    }

    bool gdr_font_file_adapter::get_metrics(const std::size_t idx, open_font_metrics &metrics) {
        if (!is_valid() || (idx >= store_.typefaces_.size())) {
            return false;
        }

        loader::gdr::typeface &the_typeface = store_.typefaces_[idx];

        for (std::size_t i = 0; i < the_typeface.font_bitmaps_.size(); i++) {
            metrics.max_width = std::max<std::int16_t>(static_cast<std::int16_t>(the_typeface.font_bitmaps_[i]->header_.max_char_width_in_pixels_), metrics.max_width);
            metrics.max_height = std::max<std::int16_t>(static_cast<std::int16_t>(the_typeface.font_bitmaps_[i]->header_.cell_height_in_pixels_), metrics.max_height);
            metrics.ascent = std::max<std::int16_t>(static_cast<std::int16_t>(the_typeface.font_bitmaps_[i]->header_.ascent_in_pixels_), metrics.ascent);
        }

        // No baseline correction
        metrics.baseline_correction = 0;                            // For the whole font
        metrics.descent = metrics.max_height - metrics.ascent;      // Correct?
        metrics.design_height = metrics.max_height;                 // Dunno, maybe wrong ;(
        metrics.max_depth = 0;                                      // Help I dunno what this is

        return true;
    }

    loader::gdr::character *gdr_font_file_adapter::get_character(const std::size_t idx, std::uint32_t code) {
        if (!is_valid() || (idx >= store_.typefaces_.size())) {
            return false;
        }

        loader::gdr::typeface &the_typeface = store_.typefaces_[idx];

        for (auto &font_bitmap: the_typeface.font_bitmaps_) {
            for (auto &code_section: font_bitmap->code_sections_) {
                if ((code_section.header_.start_ >= code) && (code_section.header_.end_ <= code)) {
                    // Found you!
                    return &code_section.chars_[code - code_section.header_.start_];
                }
            }
        }

        return nullptr;
    }
    
    bool gdr_font_file_adapter::get_glyph_metric(const std::size_t idx, std::uint32_t code, open_font_character_metric &character_metric, const std::int32_t baseline_horz_off,
        const float scale_x, const float scale_y) {
        loader::gdr::character *the_char = get_character(idx, code);

        if (!the_char) {
            return false;
        }

        const std::int16_t target_width = the_char->metric_->move_in_pixels_ - the_char->metric_->left_adj_in_pixels_ - the_char->metric_->right_adjust_in_pixels_;

        character_metric.width = static_cast<std::int16_t>(target_width * scale_x);
        character_metric.height = static_cast<std::int16_t>(the_char->metric_->height_in_pixels_ * scale_y);
        character_metric.horizontal_advance = static_cast<std::int16_t>(the_char->metric_->move_in_pixels_ * scale_x);
        character_metric.horizontal_bearing_x = static_cast<std::int16_t>(the_char->metric_->left_adj_in_pixels_ * scale_x);
        character_metric.horizontal_bearing_y = static_cast<std::int16_t>(((the_char->metric_->right_adjust_in_pixels_ == 0xFF) ?
            0 : the_char->metric_->right_adjust_in_pixels_) * scale_x);

        character_metric.vertical_bearing_x = 0;
        character_metric.vertical_bearing_y = 0;
        character_metric.bitmap_type = glyph_bitmap_type::monochrome_glyph_bitmap;

        return true;
    }

    bool gdr_font_file_adapter::does_glyph_exist(std::size_t idx, std::uint32_t code) {
        return get_character(idx, code);
    }

    std::uint8_t *gdr_font_file_adapter::get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const float scale_x,
        const float scale_y, int *rasterized_width, int *rasterized_height, epoc::glyph_bitmap_type *bmp_type) {
        loader::gdr::character *the_char = get_character(idx, code);

        if (!the_char) {
            return nullptr;
        }

        // Do simple scaling! :D If it is blocky, probably have to get a library involved
        std::vector<std::uint32_t> scaled_result;
        std::uint32_t *src = the_char->data_.data();

        const std::int16_t target_width = the_char->metric_->move_in_pixels_ - the_char->metric_->left_adj_in_pixels_ - the_char->metric_->right_adjust_in_pixels_;
        const std::int16_t scaled_width = static_cast<std::int16_t>(target_width * scale_x);
        const std::int16_t scaled_height = static_cast<std::int16_t>(the_char->metric_->height_in_pixels_ * scale_y);
        
        if ((scale_x != 1.0f) || (scale_y != 1.0f)) {
            scaled_result.resize((scaled_width * scaled_height + 31) >> 5);

            for (std::int16_t y = 0; y < scaled_height; y++) {
                for (std::int16_t x = 0; x < scaled_width; x++) {
                    const std::int16_t dx = static_cast<std::int16_t>(scale_x * x );
                    const std::int16_t dy = static_cast<std::int16_t>(scale_y * y);
                    const std::int32_t src_pixel_loc = (dy * target_width + dx);
                    const std::int32_t dest_pixel_loc = (y * scaled_width + x);

                    scaled_result[dest_pixel_loc >> 5] |= (the_char->data_[src_pixel_loc >> 5] >> (src_pixel_loc & 31)) << (dest_pixel_loc & 31);
                }
            }

            src = scaled_result.data();
        }

        // RLE this baby! Alloc this big to gurantee compressed data will always fit
        std::uint32_t *compressed_bitmap = new std::uint32_t[(scaled_width * scaled_height + 31) >> 5];
        std::int16_t total_line_processed_so_far = 0;
        std::uint32_t total_bit_write = 0;
        
        #define WRITE_BIT_32(bit) compressed_bitmap[(total_bit_write >> 5)] |= ((bit & 1) << (total_bit_write & 31)); total_bit_write++

        auto compare_line_equal = [&](std::uint32_t p_l1, std::uint32_t p_l2, const std::uint32_t n) -> bool {
            std::uint32_t left = n;

            while (left > 0) {
                std::uint32_t to_read = std::max<std::uint32_t>(left, 32);

                std::uint32_t pos1 = (p_l1 * scaled_height + n - left);
                std::uint32_t pos2 = (p_l2 * scaled_height + n - left);

                std::uint32_t maximum_1 = 32U - (pos1 & 31);
                std::uint32_t maximum_2 = 32U - (pos2 & 31);

                std::uint32_t l1p = common::extract_bits(src[pos1 >> 5], (pos1 & 31), std::min<std::uint32_t>(maximum_1, to_read)) |
                    ((maximum_1 < to_read) ? common::extract_bits(src[(pos1 >> 5) + 1], 0, to_read - maximum_1) : 0);

                std::uint32_t l2p = common::extract_bits(src[pos2 >> 5], (pos2 & 31), std::min<std::uint32_t>(maximum_2, to_read)) |
                    ((maximum_2 < to_read) ? common::extract_bits(src[(pos2 >> 5) + 1], 0, to_read - maximum_2) : 0);

                if (l1p != l2p) {
                    return false;
                }

                left -= to_read;
            }

            return true;
        };

        while (total_line_processed_so_far < scaled_height) {
            bool mode = compare_line_equal(total_line_processed_so_far, total_line_processed_so_far + 1, scaled_width);
            std::int8_t count = 2;

            while ((count < 15) && (compare_line_equal(total_line_processed_so_far, total_line_processed_so_far + count, scaled_width) == mode)) {
                count++;
            }

            WRITE_BIT_32(mode ? 0 : 1);            // Repeat mode if line equal

            // Write the repeat count
            WRITE_BIT_32(count & 1);
            WRITE_BIT_32((count >> 1) & 1);
            WRITE_BIT_32((count >> 2) & 1);
            WRITE_BIT_32((count >> 3) & 1);

            // Write the line content
            std::uint32_t loc = total_line_processed_so_far * scaled_width;

            for (std::size_t j = 0; j < (mode ? 1 : count); j++) {
                for (std::size_t i = 0; i < scaled_width; i++) {
                    // Give up being fast lol
                    WRITE_BIT_32((src[(loc + i) >> 5] >> ((loc + i) & 31)) & 1);
                }

                loc += scaled_width;
            }

            total_line_processed_so_far += count;
        }

        *bmp_type = epoc::glyph_bitmap_type::monochrome_glyph_bitmap;
        
        // In case this adapter get destroyed. It will free this data.
        dynamic_alloc_list_.push_back(compressed_bitmap);
        return reinterpret_cast<std::uint8_t*>(compressed_bitmap);
    }

    void gdr_font_file_adapter::free_glyph_bitmap(std::uint8_t *data) {
        auto store_result = std::find(dynamic_alloc_list_.begin(), dynamic_alloc_list_.end(), reinterpret_cast<std::uint32_t*>(data));

        if (store_result != dynamic_alloc_list_.end()) {
            delete data;
            dynamic_alloc_list_.erase(store_result);
        }
    }
}