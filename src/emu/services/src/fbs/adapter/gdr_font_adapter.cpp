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

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

namespace eka2l1::epoc::adapter {
    // Do you like fonts? I dont (pent0)
    gdr_font_file_adapter::gdr_font_file_adapter(std::vector<std::uint8_t> &data) {
        // Instantiate a read-only buffer stream
        buf_stream_ = std::make_unique<common::ro_buf_stream>(&data[0], data.size());
        
        if (!loader::gdr::parse_store(reinterpret_cast<common::ro_stream*>(buf_stream_.get()), store_)) {
            // Do this so a sanity check can happens
            buf_stream_.release();
        }

        // Note: Stream is unusable after constructor. It just there for sanity check.
    }

    std::size_t gdr_font_file_adapter::count() {
        return store_.typefaces_.size();
    }

    std::uint32_t gdr_font_file_adapter::unique_id(const std::size_t face_index) {
        if (face_index >= store_.typefaces_.size()) {
            return INVALID_FONT_TF_UID;
        }

        // Use the first
        return store_.typefaces_[face_index].font_bitmaps_[0]->header_.uid_;
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
            return nullptr;
        }

        loader::gdr::typeface &the_typeface = store_.typefaces_[idx];

        for (auto &font_bitmap: the_typeface.font_bitmaps_) {
            for (auto &code_section: font_bitmap->code_sections_) {
                if ((code_section.header_.start_ <= code) && (code <= code_section.header_.end_)) {
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
        const float scale_y, int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type) {
        loader::gdr::character *the_char = get_character(idx, code);

        if (!the_char) {
            return nullptr;
        }

        // Do simple scaling! :D If it is blocky, probably have to get a library involved
        std::vector<std::uint32_t> scaled_result;
        std::uint32_t *src = the_char->data_.data();

        const std::int16_t target_width = the_char->metric_->move_in_pixels_ - the_char->metric_->left_adj_in_pixels_ - the_char->metric_->right_adjust_in_pixels_;
        const std::int16_t scaled_width = static_cast<std::int16_t>(std::roundf(target_width * scale_x));
        const std::int16_t scaled_height = static_cast<std::int16_t>(std::roundf(the_char->metric_->height_in_pixels_ * scale_y));
        
        if ((scale_x != 1.0f) || (scale_y != 1.0f)) {
            scaled_result.resize((scaled_width * scaled_height + 31) >> 5);

            for (std::int16_t y = 0; y < scaled_height; y++) {
                for (std::int16_t x = 0; x < scaled_width; x++) {
                    const std::int16_t dx = static_cast<std::int16_t>(x / scale_x);
                    const std::int16_t dy = static_cast<std::int16_t>(y / scale_y);
                    const std::int32_t src_pixel_loc = (dy * target_width + dx);
                    const std::int32_t dest_pixel_loc = (y * scaled_width + x);

                    scaled_result[dest_pixel_loc >> 5] |= ((the_char->data_[src_pixel_loc >> 5] >> (src_pixel_loc & 31)) & 1) << (dest_pixel_loc & 31);
                }
            }

            src = scaled_result.data();
        }

        // RLE this baby! Alloc this big to gurantee compressed data will always fit. If the compression is bad
        // we also add 5 more words. in case compression is not effective at all.
        std::uint32_t *compressed_bitmap = new std::uint32_t[((scaled_width * scaled_height + 31) >> 5) + 5];
        std::int16_t total_line_processed_so_far = 0;
        std::uint32_t total_bit_write = 0;
        
        #define WRITE_BIT_32(bit) compressed_bitmap[(total_bit_write >> 5)] |= ((bit & 1) << (total_bit_write & 31)); total_bit_write++

        auto compare_line_equal = [&](std::uint32_t p_l1, std::uint32_t p_l2, const std::uint32_t n) -> bool {
            std::uint32_t left = n;

            while (left > 0) {
                std::uint32_t to_read = std::min<std::uint32_t>(left, 32);

                std::uint32_t pos1 = (p_l1 * scaled_width + n - left);
                std::uint32_t pos2 = (p_l2 * scaled_width + n - left);

                std::uint32_t maximum_1 = 32U - (pos1 & 31);
                std::uint32_t maximum_2 = 32U - (pos2 & 31);

                std::uint32_t part1read = std::min<std::uint32_t>(maximum_1, to_read);
                std::uint32_t part2read = std::min<std::uint32_t>(maximum_2, to_read);

                std::uint32_t l1p = common::extract_bits(src[pos1 >> 5], (pos1 & 31), part1read) |
                    ((maximum_1 < to_read) ? (common::extract_bits(src[(pos1 >> 5) + 1], 0, to_read - maximum_1) << part1read) : 0);

                std::uint32_t l2p = common::extract_bits(src[pos2 >> 5], (pos2 & 31), part2read) |
                    ((maximum_2 < to_read) ? (common::extract_bits(src[(pos2 >> 5) + 1], 0, to_read - maximum_2) << part2read) : 0);

                if (l1p != l2p) {
                    return false;
                }

                left -= to_read;
            }

            return true;
        };

        while (total_line_processed_so_far < scaled_height) {
            bool mode = false;
            std::int8_t count = 2;

            if (total_line_processed_so_far == scaled_height - 1) {
                count = 1;
                mode = false;
            } else {
                mode = compare_line_equal(total_line_processed_so_far, total_line_processed_so_far + 1, scaled_width);

                bool got_in = false;

                while ((count < 15) && (total_line_processed_so_far + count < scaled_height) && 
                    (compare_line_equal(total_line_processed_so_far + (mode ? 0 : (count - 1)), total_line_processed_so_far + count, scaled_width) == mode)) {
                    count++;
                    got_in = true;
                }

                if (got_in) {
                    count--;
                }
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

        #undef WRITE_BIT_32

        if (bmp_type)
            *bmp_type = epoc::glyph_bitmap_type::monochrome_glyph_bitmap;

        total_size = ((total_bit_write + 31) >> 5) * 4;

        if (rasterized_width) {
            *rasterized_width = scaled_width;
        }

        if (rasterized_height) {
            *rasterized_height = scaled_height;
        }

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

    bool gdr_font_file_adapter::begin_get_atlas(std::uint8_t *atlas_ptr, const eka2l1::vec2 atlas_size) {
        if (pack_context_) {
            // The atlas rendering is still active!
            return false;
        }

        pack_context_ = std::make_unique<stbrp_context>();
        pack_nodes_.resize(atlas_size.y);

        stbrp_init_target(pack_context_.get(), atlas_size.x, atlas_size.y, pack_nodes_.data(), static_cast<int>(pack_nodes_.size()));
        pack_dest_ = atlas_ptr;
        pack_size_ = atlas_size;

        return true;
    }

    bool gdr_font_file_adapter::get_glyph_atlas(const std::size_t idx, const char16_t start_code, int *unicode_point, const char16_t num_code, const int font_size,
        character_info *info) {
        std::vector<stbrp_rect> rect_build;
        std::vector<loader::gdr::character*> the_chars;
        rect_build.resize(num_code);

        for (char16_t i = 0; i < num_code; i++) {
            const char16_t ucode = (unicode_point) ? static_cast<char16_t>(unicode_point[i]) : start_code + i;
            loader::gdr::character *c = get_character(idx, ucode);

            if (!c) {
                rect_build[i].w = 0;
                rect_build[i].h = 0;
            } else {
                const float scale_factor = static_cast<float>(c->metric_->height_in_pixels_) / font_size;

                rect_build[i].w = static_cast<stbrp_coord>((c->metric_->move_in_pixels_ - c->metric_->left_adj_in_pixels_ - c->metric_->right_adjust_in_pixels_) * scale_factor);
                rect_build[i].h = font_size;
            }

            the_chars.push_back(c);
        }

        if (stbrp_pack_rects(pack_context_.get(), rect_build.data(), num_code) != 1) {
            return false;
        }

        for (char16_t i = 0; i < num_code; i++) {
            // Copy those info to character infos
            info[i].x0 = rect_build[i].x;
            info[i].y0 = rect_build[i].y;
            info[i].x1 = rect_build[i].x + rect_build[i].w;
            info[i].y1 = rect_build[i].y + rect_build[i].h;

            const float scale_factor = static_cast<float>(the_chars[i]->metric_->height_in_pixels_) / font_size;
            const std::int16_t target_width = the_chars[i]->metric_->move_in_pixels_ - the_chars[i]->metric_->left_adj_in_pixels_ -
                the_chars[i]->metric_->right_adjust_in_pixels_;

            const std::int16_t target_height = the_chars[i]->metric_->height_in_pixels_;

            info[i].xoff = the_chars[i]->metric_->left_adj_in_pixels_ * scale_factor;
            info[i].yoff = the_chars[i]->metric_->ascent_in_pixels_ * scale_factor;
            info[i].xoff2 = info[i].xoff + scale_factor * target_width;
            info[i].yoff2 =  info[i].yoff + scale_factor * target_height;
            info[i].xadv = the_chars[i]->metric_->move_in_pixels_ * scale_factor;

            loader::gdr::bitmap &bmp = the_chars[i]->data_;

            // UWU gonna copy data to you. Simple scaling algorithm WARNING!
            for (int y = rect_build[i].y; y < rect_build[i].y + rect_build[i].h; y++) {
                for (int x = rect_build[i].x; x < rect_build[i].x + rect_build[i].w; x++) {
                    const std::uint32_t src_scale_loc = (static_cast<std::int16_t>(y * scale_factor) * target_width + static_cast<std::int16_t>(x * scale_factor));
                    pack_dest_[pack_size_.x * y + x] = ((bmp[src_scale_loc >> 5] >> (src_scale_loc & 31)) & 1) * 0xFF;
                }
            }
        }

        return true;
    }

    void gdr_font_file_adapter::end_get_atlas() {
        pack_context_.reset();
        pack_dest_ = nullptr;
    }
}