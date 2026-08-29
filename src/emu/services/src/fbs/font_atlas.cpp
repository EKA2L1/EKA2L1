/*
 * Copyright (c) 2019 EKA2L1 Team
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

#include <drivers/graphics/graphics.h>
#include <services/fbs/font_atlas.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#include <common/algorithm.h>
#include <common/time.h>

#include <cstring>

namespace eka2l1::epoc {
    struct atlas_packing_state {
        std::vector<stbrp_node> nodes_;
        stbrp_context context_;
        int width_;
    };

    font_atlas::~font_atlas() = default;

    font_atlas::font_atlas()
        : atlas_handle_(0)
        , atlas_data_(nullptr) {
    }

    font_atlas::font_atlas(adapter::font_file_adapter_base *adapter, const std::size_t typeface_idx, const char16_t initial_start,
        const char16_t initial_char_count, const int font_size, const std::uint32_t metric_identifier)
        : atlas_handle_(0)
        , adapter_(adapter)
        , metric_identifier_(metric_identifier)
        , size_(font_size)
        , initial_range_(initial_start, initial_char_count)
        , typeface_idx_(typeface_idx)
        , atlas_data_(nullptr) {
    }

    void font_atlas::init(adapter::font_file_adapter_base *adapter, const std::size_t typeface_idx, const char16_t initial_start,
        const char16_t initial_char_count, const int font_size, const std::uint32_t metric_identifier) {
        adapter_ = adapter;
        atlas_handle_ = 0;
        metric_identifier_ = metric_identifier;
        size_ = font_size;
        initial_range_ = { initial_start, initial_char_count };
        typeface_idx_ = typeface_idx;
        pack_state_.reset();

        atlas_data_.reset();
    }

    void font_atlas::destroy(drivers::graphics_driver *driver) {
        if (atlas_handle_) {
            drivers::graphics_command_builder builder;
            builder.destroy_bitmap(atlas_handle_);

            drivers::command_list retrieved = builder.retrieve_command_list();
            driver->submit_command_list(retrieved);

            atlas_handle_ = 0;
            atlas_data_.reset();
        }

        pack_state_.reset();

        last_use_.clear();
        characters_.clear();
    }

    int font_atlas::get_atlas_width() const {
        return common::align(ESTIMATE_MAX_CHAR_IN_ATLAS_WIDTH * size_, 1024);
    }

    bool font_atlas::begin_packing(const int width) {
        if (!pack_state_) {
            pack_state_ = std::make_unique<atlas_packing_state>();
        }

        // stb wants one node per atlas column to place rectangles precisely.
        pack_state_->nodes_.resize(width);
        pack_state_->width_ = width;

        stbrp_init_target(&pack_state_->context_, width, width, pack_state_->nodes_.data(),
            static_cast<int>(pack_state_->nodes_.size()));

        return true;
    }

    bool font_atlas::pack_glyphs(const std::vector<int> &codes, adapter::character_info *infos) {
        if (!pack_state_ || codes.empty()) {
            return (pack_state_ != nullptr);
        }

        std::vector<eka2l1::vec2> sizes(codes.size());

        if (!adapter_->measure_atlas_glyphs(typeface_idx_, codes.data(), codes.size(), metric_identifier_,
                sizes.data())) {
            return false;
        }

        std::vector<stbrp_rect> rects(codes.size());

        for (std::size_t i = 0; i < codes.size(); i++) {
            rects[i].x = 0;
            rects[i].y = 0;
            rects[i].w = static_cast<stbrp_coord>(sizes[i].x + GLYPH_PADDING * 2);
            rects[i].h = static_cast<stbrp_coord>(sizes[i].y + GLYPH_PADDING * 2);
        }

        if (!stbrp_pack_rects(&pack_state_->context_, rects.data(), static_cast<int>(rects.size()))) {
            // Out of room. The caller decides whether to rebuild the atlas.
            return false;
        }

        std::vector<eka2l1::vec2> positions(codes.size());

        for (std::size_t i = 0; i < codes.size(); i++) {
            positions[i] = eka2l1::vec2(rects[i].x + GLYPH_PADDING, rects[i].y + GLYPH_PADDING);
        }

        return adapter_->render_atlas_glyphs(typeface_idx_, codes.data(), codes.size(), metric_identifier_,
            atlas_data_.get(), { pack_state_->width_, pack_state_->width_ }, positions.data(), infos);
    }

    bool font_atlas::draw_text(const std::u16string &text, const eka2l1::rect &text_box, const epoc::text_alignment alignment, drivers::graphics_driver *driver, drivers::graphics_command_builder &builder, const eka2l1::vec2f scale_vector) {
        // Clamp the atlas to what the GPU can actually allocate. Large fonts
        // (e.g. high display-scale rendering) would otherwise request an atlas
        // bigger than GL_MAX_TEXTURE_SIZE; the create then fails and the
        // incomplete atlas samples as opaque black, turning every glyph drawn
        // from it into a solid block (seen on the iOS GLES simulator, max 4096).
        const int width = common::min<int>(get_atlas_width(),
            static_cast<int>(driver->max_texture_size()));
        drivers::graphics_command_builder upload_builder;

        if (!atlas_data_) {
            atlas_data_ = std::make_unique<std::uint8_t[]>(width * width * adapter_->get_atlas_bitmap_bits_per_pixel() / 8);

            std::vector<int> initial_codes(initial_range_.second);

            for (char16_t i = 0; i < initial_range_.second; i++) {
                initial_codes[i] = initial_range_.first + i;
            }

            std::vector<adapter::character_info> cinfos(initial_codes.size());

            begin_packing(width);

            if (!pack_glyphs(initial_codes, cinfos.data())) {
                return false;
            }

            // initialize the last used list and character map
            for (std::size_t i = 0; i < initial_codes.size(); i++) {
                last_use_.push_back(initial_codes[i]);
                characters_.emplace(static_cast<char16_t>(initial_codes[i]), cinfos[i]);
            }

            // Submit the bitmap through another queue, in case the command list above never got submitted
            atlas_handle_ = drivers::create_bitmap(driver, { width, width },  adapter_->get_atlas_bitmap_bits_per_pixel());

            upload_builder.update_bitmap(atlas_handle_, reinterpret_cast<const char *>(atlas_data_.get()),
                width * width * adapter_->get_atlas_bitmap_bits_per_pixel() / 8, { 0, 0 }, { width, width });
            upload_builder.set_texture_filter(atlas_handle_, false, drivers::filter_option::nearest);
        }

        std::vector<int> to_rast;
        std::vector<char16_t> unique_char;

        // Iterate through characters, and filter out characters which is not available in the atlas.
        // Add character to last used, too.
        for (auto &chr : text) {
            if (characters_.find(chr) == characters_.end()) {
                if (!std::binary_search(to_rast.begin(), to_rast.end(), chr)) {
                    to_rast.push_back(chr);
                    std::sort(to_rast.begin(), to_rast.end());
                }
            }

            if (std::find(unique_char.begin(), unique_char.end(), chr) == unique_char.end()) {
                last_use_.insert(last_use_.begin(), chr);
                unique_char.push_back(chr);
            }
        }

        last_use_.erase(last_use_.end() - unique_char.size(), last_use_.end());

        if (!to_rast.empty()) {
            std::vector<adapter::character_info> cinfos(to_rast.size());

            if (pack_glyphs(to_rast, cinfos.data())) {
                for (std::size_t i = 0; i < to_rast.size(); i++) {
                    characters_.emplace(static_cast<char16_t>(to_rast[i]), cinfos[i]);
                }
            } else {
                // Out of room: rebuild the atlas from the characters used most
                // recently, plus the ones that did not fit. `last_use_` has the
                // hottest first, so dropping its tail evicts the coldest.
                std::vector<int> rebuild(last_use_.begin(),
                    last_use_.begin() + common::min<std::size_t>(last_use_.size(), characters_.size()));

                rebuild.erase(std::remove_if(rebuild.begin(), rebuild.end(), [&](const int code) {
                    return characters_.find(static_cast<char16_t>(code)) == characters_.end();
                }), rebuild.end());

                if (rebuild.size() > EVICT_ON_REBUILD) {
                    rebuild.resize(rebuild.size() - EVICT_ON_REBUILD);
                }

                rebuild.insert(rebuild.end(), to_rast.begin(), to_rast.end());

                std::vector<adapter::character_info> rebuilt(rebuild.size());

                std::memset(atlas_data_.get(), 0,
                    width * width * adapter_->get_atlas_bitmap_bits_per_pixel() / 8);
                begin_packing(width);

                if (!pack_glyphs(rebuild, rebuilt.data())) {
                    return false;
                }

                characters_.clear();

                for (std::size_t i = 0; i < rebuild.size(); i++) {
                    characters_[static_cast<char16_t>(rebuild[i])] = rebuilt[i];
                }
            }

            upload_builder.update_bitmap(atlas_handle_, reinterpret_cast<const char *>(atlas_data_.get()),
                width * width * adapter_->get_atlas_bitmap_bits_per_pixel() / 8, { 0, 0 }, { width, width });
        }

        eka2l1::vec2 cur_pos = text_box.top;

        // Calculate size of the text to know where to put them
        // If other alignment then left is on
        if (alignment != epoc::text_alignment::left) {
            float size_length = 0;

            for (auto &chr : text) {
                size_length += static_cast<int>(characters_[chr].xadv * scale_vector[0]);
            }

            if (alignment == epoc::text_alignment::right) {
                cur_pos.x = text_box.size.x + text_box.top.x - static_cast<int>(size_length);
            } else {
                cur_pos.x += static_cast<int>((text_box.size.x - size_length) / 2);
            }
        }

        builder.set_feature(drivers::graphics_feature::blend, true);
        builder.blend_formula(drivers::blend_equation::add, drivers::blend_equation::add,
            drivers::blend_factor::frag_out_alpha, drivers::blend_factor::one_minus_frag_out_alpha,
            drivers::blend_factor::one, drivers::blend_factor::one);

        // Start to render these texts.
        for (auto &chr : text) {
            if ((chr >= 0x200c && chr <= 0x200f) || (chr >= 0x202a && chr <= 0x202e) || (chr >= 0xfffe && chr <= 0xffff)) {
                // Skip control characters
                // TODO: Handle them properly
                continue;
            }

            eka2l1::rect source_rect;
            adapter::character_info &info = characters_[chr];

            source_rect.top = { info.x0, info.y0 };
            source_rect.size = eka2l1::object_size(info.x1 - info.x0, info.y1 - info.y0);

            eka2l1::rect dest_rect;
            dest_rect.top.x = cur_pos.x + static_cast<int>(info.xoff * scale_vector[0]);
            dest_rect.top.y = cur_pos.y + static_cast<int>(info.yoff * scale_vector[1]);

            dest_rect.size.x = static_cast<int>((info.xoff2 - info.xoff) * scale_vector[0]);
            dest_rect.size.y = static_cast<int>((info.yoff2 - info.yoff) * scale_vector[1]);

            if ((dest_rect.size.x != 0) && (dest_rect.size.y != 0) && (source_rect.size.x != 0) && (source_rect.size.y != 0)) {
                builder.draw_bitmap(atlas_handle_, 0, dest_rect, source_rect, eka2l1::vec2(0, 0), 0.0f,
                    drivers::bitmap_draw_flag_use_brush);
            }

            // TODO: Newline
            cur_pos.x += static_cast<int>(std::round(info.xadv * scale_vector[0]));
        }

        builder.set_feature(drivers::graphics_feature::blend, false);

        drivers::command_list retrieved = upload_builder.retrieve_command_list();
        driver->submit_command_list(retrieved);

        return true;
    }
}