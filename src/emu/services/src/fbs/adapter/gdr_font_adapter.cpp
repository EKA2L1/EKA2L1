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

#include <common/bytes.h>
#include <common/log.h>
#include <services/fbs/adapter/gdr_font_adapter.h>

namespace eka2l1::epoc::adapter {
    static open_font_metrics build_of_metrics_from_font_bitmap(const loader::gdr::font_bitmap *target_bitmap) {
        open_font_metrics metrics;

        metrics.max_height = 0;
        metrics.max_width = 0;
        metrics.ascent = 0;

        metrics.max_width = target_bitmap->header_.max_char_width_in_pixels_;
        metrics.max_height = target_bitmap->header_.cell_height_in_pixels_;
        metrics.ascent = target_bitmap->header_.ascent_in_pixels_;

        // No baseline correction
        metrics.baseline_correction = 0; // For the whole font
        metrics.descent = -(metrics.ascent - metrics.max_height); // Correct?
        metrics.design_height = metrics.max_height; // Dunno, maybe wrong ;(
        metrics.max_depth = 0; // Help I dunno what this is

        return metrics;
    }

    gdr_font_file_adapter::gdr_font_file_adapter(std::vector<std::uint8_t> &data) {
        // Instantiate a read-only buffer stream
        buf_stream_ = std::make_unique<common::ro_buf_stream>(&data[0], data.size());

        if (!loader::gdr::parse_store(reinterpret_cast<common::ro_stream *>(buf_stream_.get()), store_)) {
            // Do this so a sanity check can happens
            buf_stream_.release();
        }

        // Note: Stream is unusable after constructor. It just there for sanity check.
    }

    gdr_font_file_adapter::~gdr_font_file_adapter() {
        for (auto &alloc : dynamic_alloc_list_) {
            delete alloc;
        }
    }

    std::size_t gdr_font_file_adapter::count() {
        return store_.typefaces_.size();
    }

    std::optional<open_font_metrics> gdr_font_file_adapter::get_metric_with_uid(const std::size_t face_index, const std::uint32_t uid,
        std::uint32_t *metric_identifier) {
        if (face_index >= store_.typefaces_.size()) {
            return std::nullopt;
        }

        // Use the first
        for (std::size_t i = 0; i < store_.typefaces_[face_index].font_bitmaps_.size(); i++) {
            if (store_.typefaces_[face_index].font_bitmaps_[i]->header_.uid_ == uid) {
                if (metric_identifier) {
                    *metric_identifier = static_cast<std::uint32_t>(i);
                }

                return build_of_metrics_from_font_bitmap(store_.typefaces_[face_index].font_bitmaps_[i]);
            }
        }

        return std::nullopt;
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
        face_attrib.style = 0;

        if (the_typeface.header_.flags_ & epoc::typeface_info::tf_serif) {
            face_attrib.style |= open_font_face_attrib::serif;
        }

        if (the_typeface.analysed_style_ & loader::gdr::typeface::FLAG_BOLD) {
            face_attrib.style |= open_font_face_attrib::bold;
        }

        if (the_typeface.analysed_style_ & loader::gdr::typeface::FLAG_ITALIC) {
            face_attrib.style |= open_font_face_attrib::italic;
        }

        std::memcpy(face_attrib.coverage, the_typeface.whole_coverage_, sizeof(face_attrib.coverage));
        return true;
    }

    const loader::gdr::character *gdr_font_file_adapter::get_character(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) {
        if (!is_valid() || (idx >= store_.typefaces_.size())) {
            return nullptr;
        }

        if (metric_identifier >= store_.typefaces_[idx].font_bitmaps_.size()) {
            return nullptr;
        }

        loader::gdr::font_bitmap *bitmap = store_.typefaces_[idx].font_bitmaps_[metric_identifier];

        for (auto &code_section : bitmap->code_sections_) {
            if ((code_section.header_.start_ <= code) && (code <= code_section.header_.end_)) {
                // Found you!
                return &code_section.chars_[code - code_section.header_.start_];
            }
        }

        return nullptr;
    }

    bool gdr_font_file_adapter::does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) {
        return get_character(idx, code, 0);
    }

    std::uint8_t *gdr_font_file_adapter::get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier,
        int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type,
        open_font_character_metric &character_metric) {
        const loader::gdr::character *the_char = get_character(idx, code, metric_identifier);

        if (!the_char) {
            return nullptr;
        }

        // Do simple scaling! :D If it is blocky, probably have to get a library involved
        std::vector<std::uint32_t> scaled_result;
        const std::uint32_t *src = the_char->data_.data();
        const std::int16_t target_width = the_char->metric_->move_in_pixels_ - the_char->metric_->left_adj_in_pixels_ - the_char->metric_->right_adjust_in_pixels_;
        const std::int16_t target_height = the_char->metric_->height_in_pixels_;

        // Alloc this big to gurantee compressed data will always fit. If the compression is bad
        // we also add 5 more words. in case compression is not effective at all.
        const std::size_t total_compressed_word = monochrome_glyph_word_count(target_width, target_height);
        std::uint32_t *compressed_bitmap = new std::uint32_t[total_compressed_word];
        std::fill(compressed_bitmap, compressed_bitmap + total_compressed_word, 0);

        const std::uint32_t total_bit_write = compress_monochrome_glyph(src, target_width, target_height,
            compressed_bitmap);

        if (bmp_type)
            *bmp_type = epoc::glyph_bitmap_type::monochrome_glyph_bitmap;

        total_size = ((total_bit_write + 31) >> 5) * 4;

        if (rasterized_width) {
            *rasterized_width = target_width;
        }

        if (rasterized_height) {
            *rasterized_height = target_height;
        }

        character_metric.width = target_width;
        character_metric.height = the_char->metric_->height_in_pixels_;
        character_metric.horizontal_advance = the_char->metric_->move_in_pixels_;
        character_metric.horizontal_bearing_x = the_char->metric_->left_adj_in_pixels_;
        character_metric.horizontal_bearing_y = the_char->metric_->ascent_in_pixels_;

        // Todo supply vertical bearing: This is spaces when text placed vertically
        character_metric.vertical_bearing_x = 0;
        character_metric.vertical_bearing_y = 0;

        character_metric.bitmap_type = glyph_bitmap_type::monochrome_glyph_bitmap;

        // In case this adapter get destroyed. It will free this data.
        dynamic_alloc_list_.push_back(compressed_bitmap);
        return reinterpret_cast<std::uint8_t *>(compressed_bitmap);
    }

    void gdr_font_file_adapter::free_glyph_bitmap(std::uint8_t *data) {
        auto store_result = std::find(dynamic_alloc_list_.begin(), dynamic_alloc_list_.end(), reinterpret_cast<std::uint32_t *>(data));

        if (store_result != dynamic_alloc_list_.end()) {
            delete data;
            dynamic_alloc_list_.erase(store_result);
        }
    }

    bool gdr_font_file_adapter::measure_atlas_glyphs(const std::size_t idx, const int *codes, const std::size_t count,
        const std::uint32_t metric_identifier, eka2l1::vec2 *sizes) {
        for (std::size_t i = 0; i < count; i++) {
            const loader::gdr::character *c = get_character(idx, static_cast<std::uint32_t>(codes[i]), metric_identifier);

            if (!c) {
                sizes[i] = eka2l1::vec2(0, 0);
                continue;
            }

            sizes[i] = eka2l1::vec2(c->metric_->move_in_pixels_ - c->metric_->left_adj_in_pixels_
                    - c->metric_->right_adjust_in_pixels_,
                c->metric_->height_in_pixels_);
        }

        return true;
    }

    bool gdr_font_file_adapter::render_atlas_glyphs(const std::size_t idx, const int *codes, const std::size_t count,
        const std::uint32_t metric_identifier, std::uint8_t *atlas, const eka2l1::vec2 atlas_size,
        const eka2l1::vec2 *positions, character_info *info) {
        for (std::size_t i = 0; i < count; i++) {
            const loader::gdr::character *c = get_character(idx, static_cast<std::uint32_t>(codes[i]), metric_identifier);

            if (!c) {
                info[i] = character_info{};
                continue;
            }

            const std::int16_t target_width = c->metric_->move_in_pixels_ - c->metric_->left_adj_in_pixels_
                - c->metric_->right_adjust_in_pixels_;
            const std::int16_t target_height = c->metric_->height_in_pixels_;

            info[i].x0 = static_cast<std::uint16_t>(positions[i].x);
            info[i].y0 = static_cast<std::uint16_t>(positions[i].y);
            info[i].x1 = static_cast<std::uint16_t>(positions[i].x + target_width);
            info[i].y1 = static_cast<std::uint16_t>(positions[i].y + target_height);

            info[i].xoff = c->metric_->left_adj_in_pixels_;
            info[i].yoff = -(c->metric_->ascent_in_pixels_);
            info[i].xoff2 = info[i].xoff + target_width;
            info[i].yoff2 = info[i].yoff + target_height;
            info[i].xadv = c->metric_->move_in_pixels_;

            const loader::gdr::bitmap &bmp = c->data_;

            for (std::int16_t y = 0; y < target_height; y++) {
                for (std::int16_t x = 0; x < target_width; x++) {
                    const std::uint32_t src_loc = static_cast<std::uint32_t>(y) * target_width + x;
                    std::uint8_t *dest = atlas + (positions[i].y + y) * atlas_size.x + positions[i].x + x;

                    if (src_loc >= static_cast<std::uint32_t>(target_width * target_height)) {
                        *dest = 0;
                    } else {
                        *dest = static_cast<std::uint8_t>(((bmp[src_loc >> 5] >> (src_loc & 31)) & 1) * 0xFF);
                    }
                }
            }
        }

        return true;
    }

    
    bool gdr_font_file_adapter::has_character(const std::size_t face_index, const std::int32_t codepoint, const std::uint32_t metric_identifier) {
        return (get_character(face_index, codepoint, metric_identifier) != nullptr);
    }

    std::uint32_t gdr_font_file_adapter::get_glyph_advance(const std::size_t face_index, const std::uint32_t codepoint, const std::uint32_t metric_identifier, const bool vertical) {
        const loader::gdr::character *the_char = get_character(face_index, codepoint, metric_identifier);
        if (!the_char) {
            return 0xFFFFFFFF;
        }

        if (vertical) {
            // Not official, GDR does not have vertical advance fields
            return the_char->metric_->height_in_pixels_;
        }

        return the_char->metric_->move_in_pixels_;
    }

    std::optional<open_font_metrics> gdr_font_file_adapter::get_nearest_supported_metric(const std::size_t face_index, const std::uint16_t targeted_font_size,
        std::uint32_t *metric_identifier, bool is_design_font_size) {
        if ((face_index >= store_.typefaces_.size()) || !is_valid()) {
            LOG_ERROR(SERVICE_FBS, "The font is not ready or the face index is out of bounds!");
            return std::nullopt;
        }

        std::int16_t min_delta = std::numeric_limits<std::int16_t>::max();
        const loader::gdr::font_bitmap *target_bitmap = nullptr;
        std::size_t final_index = 0;

        loader::gdr::typeface face = store_.typefaces_[face_index];
        for (std::size_t i = 0; i < face.font_bitmaps_.size(); i++) {
            const std::int16_t delta = (static_cast<std::int16_t>(face.font_bitmaps_[i]->header_.cell_height_in_pixels_) - static_cast<std::int16_t>(targeted_font_size));
            if (delta < min_delta) {
                target_bitmap = face.font_bitmaps_[i];
                min_delta = delta;
                final_index = i;
            }
        }

        if (target_bitmap == nullptr) {
            return std::nullopt;
        }
        
        if (metric_identifier != nullptr) {
            *metric_identifier = static_cast<std::uint32_t>(final_index);
        }

        return build_of_metrics_from_font_bitmap(target_bitmap);
    }
}
