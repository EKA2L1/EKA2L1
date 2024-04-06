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

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

namespace eka2l1::epoc::adapter {
    static bool is_gdr_pack_context_free(gdr_font_atlas_pack_context &ctx) {
        return (ctx.pack_dest_ == nullptr);
    }

    static void free_gdr_pack_context(gdr_font_atlas_pack_context &ctx) {
        ctx.pack_nodes_.clear();
        ctx.pack_context_.reset();
        ctx.pack_dest_ = nullptr;
    }

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

    gdr_font_file_adapter::gdr_font_file_adapter(std::vector<std::uint8_t> &data)
        : pack_contexts_(is_gdr_pack_context_free, free_gdr_pack_context) {
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

    bool gdr_font_file_adapter::get_glyph_metric(const std::size_t idx, std::uint32_t code, open_font_character_metric &character_metric, const std::int32_t baseline_horz_off,
        const std::uint32_t metric_identifier) {
        const loader::gdr::character *the_char = get_character(idx, code, metric_identifier);

        if (!the_char) {
            return false;
        }

        const std::int16_t target_width = the_char->metric_->move_in_pixels_ - the_char->metric_->left_adj_in_pixels_ - the_char->metric_->right_adjust_in_pixels_;

        character_metric.width = target_width;
        character_metric.height = the_char->metric_->height_in_pixels_;
        character_metric.horizontal_advance = the_char->metric_->move_in_pixels_;
        character_metric.horizontal_bearing_x = the_char->metric_->left_adj_in_pixels_;
        character_metric.horizontal_bearing_y = the_char->metric_->ascent_in_pixels_;

        // Todo supply vertical bearing: This is spaces when text placed vertically
        character_metric.vertical_bearing_x = 0;
        character_metric.vertical_bearing_y = 0;

        character_metric.bitmap_type = glyph_bitmap_type::monochrome_glyph_bitmap;

        return true;
    }

    bool gdr_font_file_adapter::does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) {
        return get_character(idx, code, 0);
    }

    std::uint8_t *gdr_font_file_adapter::get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier,
        int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type) {
        const loader::gdr::character *the_char = get_character(idx, code, metric_identifier);

        if (!the_char) {
            return nullptr;
        }


        // Do simple scaling! :D If it is blocky, probably have to get a library involved
        std::vector<std::uint32_t> scaled_result;
        const std::uint32_t *src = the_char->data_.data();
        const std::int16_t target_width = the_char->metric_->move_in_pixels_ - the_char->metric_->left_adj_in_pixels_ - the_char->metric_->right_adjust_in_pixels_;
        const std::int16_t target_height = the_char->metric_->height_in_pixels_;

        // RLE this baby! Alloc this big to gurantee compressed data will always fit. If the compression is bad
        // we also add 5 more words. in case compression is not effective at all.
        const std::size_t total_compressed_word = ((static_cast<std::uint32_t>(target_width * target_height) + 31) >> 5) + 5;
        std::uint32_t *compressed_bitmap = new std::uint32_t[total_compressed_word];
        std::fill(compressed_bitmap, compressed_bitmap + total_compressed_word, 0);

        std::int16_t total_line_processed_so_far = 0;
        std::uint32_t total_bit_write = 0;

#define WRITE_BIT_32(bit)                                                               \
    compressed_bitmap[(total_bit_write >> 5)] |= ((bit & 1) << (total_bit_write & 31)); \
    total_bit_write++

        auto compare_line_equal = [&](std::uint32_t p_l1, std::uint32_t p_l2, const std::uint32_t n) -> bool {
            std::uint32_t left = n;

            while (left > 0) {
                std::uint32_t to_read = std::min<std::uint32_t>(left, 32);

                std::uint32_t pos1 = (p_l1 * target_width + n - left);
                std::uint32_t pos2 = (p_l2 * target_width + n - left);

                std::uint32_t maximum_1 = 32U - (pos1 & 31);
                std::uint32_t maximum_2 = 32U - (pos2 & 31);

                std::uint32_t part1read = std::min<std::uint32_t>(maximum_1, to_read);
                std::uint32_t part2read = std::min<std::uint32_t>(maximum_2, to_read);

                std::uint32_t l1p = common::extract_bits(src[pos1 >> 5], (pos1 & 31), part1read) | ((maximum_1 < to_read) ? (common::extract_bits(src[(pos1 >> 5) + 1], 0, to_read - maximum_1) << part1read) : 0);

                std::uint32_t l2p = common::extract_bits(src[pos2 >> 5], (pos2 & 31), part2read) | ((maximum_2 < to_read) ? (common::extract_bits(src[(pos2 >> 5) + 1], 0, to_read - maximum_2) << part2read) : 0);

                if (l1p != l2p) {
                    return false;
                }

                left -= to_read;
            }

            return true;
        };

        while (total_line_processed_so_far < target_height) {
            bool mode = false;
            std::int8_t count = 2;

            if (total_line_processed_so_far == (target_height - 1)) {
                count = 1;
                mode = false;
            } else {
                mode = compare_line_equal(total_line_processed_so_far, total_line_processed_so_far + 1, target_width);

                bool got_in = false;

                while ((count < 15) && (total_line_processed_so_far + count < target_height) && (compare_line_equal(total_line_processed_so_far + (mode ? 0 : (count - 1)), total_line_processed_so_far + count, target_width) == mode)) {
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
            std::uint32_t loc = total_line_processed_so_far * target_width;

            for (std::size_t j = 0; j < (mode ? 1 : count); j++) {
                for (std::size_t i = 0; i < target_width; i++) {
                    // Give up being fast lol
                    WRITE_BIT_32((src[(loc + i) >> 5] >> ((loc + i) & 31)) & 1);
                }

                loc += target_width;
            }

            total_line_processed_so_far += count;
        }
#undef WRITE_BIT_32

        if (bmp_type)
            *bmp_type = epoc::glyph_bitmap_type::monochrome_glyph_bitmap;

        total_size = ((total_bit_write + 31) >> 5) * 4;

        if (rasterized_width) {
            *rasterized_width = target_width;
        }

        if (rasterized_height) {
            *rasterized_height = target_height;
        }

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

    std::int32_t gdr_font_file_adapter::begin_get_atlas(std::uint8_t *atlas_ptr, const eka2l1::vec2 atlas_size) {
        gdr_font_atlas_pack_context context;
        context.pack_context_ = std::make_unique<stbrp_context>();
        context.pack_nodes_.resize(atlas_size.y);

        stbrp_init_target(context.pack_context_.get(), atlas_size.x, atlas_size.y, context.pack_nodes_.data(), static_cast<int>(context.pack_nodes_.size()));
        context.pack_dest_ = atlas_ptr;
        context.pack_size_ = atlas_size;

        return static_cast<std::int32_t>(pack_contexts_.add(context));
    }

    static float calculate_scale_factor_of_font(const int target_size, const int font_general_height, const int char_height) {
        return static_cast<float>(target_size) / ((char_height == 0) ? static_cast<float>(font_general_height) : static_cast<float>(char_height));
    }

    bool gdr_font_file_adapter::get_glyph_atlas(const std::int32_t handle, const std::size_t idx, const char16_t start_code, int *unicode_point, const char16_t num_code,
        const std::uint32_t metric_identifier, character_info *info) {
        gdr_font_atlas_pack_context *context = pack_contexts_.get(handle);

        if (!context) {
            return false;
        }

        std::vector<stbrp_rect> rect_build;
        std::vector<const loader::gdr::character *> the_chars;
        rect_build.resize(num_code);

        for (char16_t i = 0; i < num_code; i++) {
            const char16_t ucode = (unicode_point) ? static_cast<char16_t>(unicode_point[i]) : start_code + i;
            const loader::gdr::character *c = get_character(idx, ucode, metric_identifier);

            rect_build[i].x = 0;
            rect_build[i].y = 0;

            if (!c) {
                rect_build[i].w = 0;
                rect_build[i].h = 0;
            } else {
                rect_build[i].w = static_cast<stbrp_coord>(c->metric_->move_in_pixels_ - c->metric_->left_adj_in_pixels_ - c->metric_->right_adjust_in_pixels_);
                rect_build[i].h = c->metric_->height_in_pixels_;
            }

            the_chars.push_back(c);
        }

        if (stbrp_pack_rects(context->pack_context_.get(), rect_build.data(), num_code) == 0) {
            return false;
        }

        for (char16_t i = 0; i < num_code; i++) {
            if (the_chars[i]) {
                // Copy those info to character infos
                info[i].x0 = rect_build[i].x;
                info[i].y0 = rect_build[i].y;
                info[i].x1 = rect_build[i].x + rect_build[i].w;
                info[i].y1 = rect_build[i].y + the_chars[i]->metric_->height_in_pixels_;

                const std::int16_t target_width = the_chars[i]->metric_->move_in_pixels_ - the_chars[i]->metric_->left_adj_in_pixels_ - the_chars[i]->metric_->right_adjust_in_pixels_;

                info[i].xoff = the_chars[i]->metric_->left_adj_in_pixels_;
                info[i].yoff = -(the_chars[i]->metric_->ascent_in_pixels_);
                info[i].xoff2 = info[i].xoff + target_width;
                info[i].yoff2 = info[i].yoff + (the_chars[i]->metric_->height_in_pixels_);
                info[i].xadv = the_chars[i]->metric_->move_in_pixels_;

                const loader::gdr::bitmap &bmp = the_chars[i]->data_;

                // UWU gonna copy data to you. Simple scaling algorithm WARNING!
                for (int y = rect_build[i].y; y < rect_build[i].y + rect_build[i].h; y++) {
                    for (int x = rect_build[i].x; x < rect_build[i].x + rect_build[i].w; x++) {
                        const float y_in_rect = static_cast<float>(y - rect_build[i].y);
                        const float x_in_rect = static_cast<float>(x - rect_build[i].x);

                        const std::uint32_t src_scale_loc = (static_cast<std::int16_t>(y_in_rect) * target_width + static_cast<std::int16_t>(x_in_rect));
                        if (src_scale_loc >= static_cast<std::uint32_t>(target_width * the_chars[i]->metric_->height_in_pixels_)) {
                            // Nothing, just black
                            context->pack_dest_[context->pack_size_.x * y + x] = 0;
                        } else {
                            context->pack_dest_[context->pack_size_.x * y + x] = ((bmp[src_scale_loc >> 5] >> (src_scale_loc & 31)) & 1) * 0xFF;
                        }
                    }
                }
            } else {
                info[i].xoff = 0;
                info[i].xoff2 = 0;
                info[i].yoff = 0;
                info[i].yoff2 = 0;
                info[i].xadv = 0;
                info[i].x0 = 0;
                info[i].y0 = 0;
                info[i].x1 = 0;
                info[i].y1 = 0;
            }
        }

        return true;
    }

    void gdr_font_file_adapter::end_get_atlas(const std::int32_t handle) {
        pack_contexts_.remove(handle);
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
