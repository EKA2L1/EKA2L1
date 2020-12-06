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

#include <common/algorithm.h>
#include <common/time.h>

namespace eka2l1::epoc {
    font_atlas::font_atlas()
        : atlas_handle_(0)
        , atlas_data_(nullptr)
        , pack_handle_(0) {
    }

    font_atlas::font_atlas(adapter::font_file_adapter_base *adapter, const std::size_t typeface_idx, const char16_t initial_start,
        const char16_t initial_char_count, int font_size)
        : atlas_handle_(0)
        , adapter_(adapter)
        , size_(font_size)
        , initial_range_(initial_start, initial_char_count)
        , typeface_idx_(typeface_idx)
        , atlas_data_(nullptr)
        , pack_handle_(0) {
    }

    void font_atlas::init(adapter::font_file_adapter_base *adapter, const std::size_t typeface_idx, const char16_t initial_start,
        const char16_t initial_char_count, int font_size) {
        adapter_ = adapter;
        atlas_handle_ = 0;
        size_ = font_size;
        initial_range_ = { initial_start, initial_char_count };
        typeface_idx_ = typeface_idx;
        pack_handle_ = 0;

        atlas_data_.reset();
    }

    void font_atlas::free(drivers::graphics_driver *driver) {
        if (atlas_handle_) {
            auto cmd_list = driver->new_command_list();
            auto cmd_builder = driver->new_command_builder(cmd_list.get());

            cmd_builder->destroy_bitmap(atlas_handle_);
            driver->submit_command_list(*cmd_list);

            atlas_handle_ = 0;
            atlas_data_.reset();
        }

        if (pack_handle_) {
            adapter_->end_get_atlas(pack_handle_);
        }
    }

    int font_atlas::get_atlas_width() const {
        return common::align(ESTIMATE_MAX_CHAR_IN_ATLAS_WIDTH * size_, 1024);
    }

    bool font_atlas::draw_text(const std::u16string &text, const eka2l1::rect &text_box, const epoc::text_alignment alignment, drivers::graphics_driver *driver, drivers::graphics_command_list_builder *builder) {
        const int width = get_atlas_width();

        if (!atlas_data_) {
            atlas_data_ = std::make_unique<std::uint8_t[]>(width * width);
            auto cinfos = std::make_unique<adapter::character_info[]>(initial_range_.second);

            pack_handle_ = adapter_->begin_get_atlas(atlas_data_.get(), { width, width });
            
            if (pack_handle_ == -1) {
                return false;
            }

            if (!adapter_->get_glyph_atlas(pack_handle_, typeface_idx_, initial_range_.first, nullptr, initial_range_.second,
                    size_, cinfos.get())) {
                return false;
            }

            // initialize the last used list and character map
            for (char16_t i = 0; i < initial_range_.second; i++) {
                last_use_.push_back(initial_range_.first + i);
                characters_.emplace(initial_range_.first + i, cinfos[i]);
            }

            atlas_handle_ = drivers::create_bitmap(driver, { width, width }, 8);
            builder->update_bitmap(atlas_handle_, reinterpret_cast<const char *>(atlas_data_.get()),
                width * width, { 0, 0 }, { width, width });
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
            // Try to rasterize these
            auto cinfos = std::make_unique<adapter::character_info[]>(to_rast.size());

            if (!adapter_->get_glyph_atlas(pack_handle_, typeface_idx_, 0, to_rast.data(), static_cast<char16_t>(to_rast.size()), size_,
                    cinfos.get())) {
                // Try to redo the atlas, getting latest use characters.
                adapter_->end_get_atlas(pack_handle_);
                pack_handle_ = adapter_->begin_get_atlas(atlas_data_.get(), { width, width });

                if (pack_handle_ == -1) {
                    return false;
                }

                if (!adapter_->get_glyph_atlas(pack_handle_, typeface_idx_, 0, &last_use_[0], static_cast<char16_t>(characters_.size() - 5),
                        size_, cinfos.get())) {
                    return false;
                }

                for (std::size_t i = 0; i < characters_.size() - 5; i++) {
                    characters_[last_use_[i]] = cinfos[i];
                }
            }
        }

        eka2l1::vec2 cur_pos = text_box.top;

        // Calculate size of the text to know where to put them
        // If other alignment then left is on
        if (alignment != epoc::text_alignment::left) {
            float size_length = 0;

            for (auto &chr : text) {
                size_length += characters_[chr].xoff2 - characters_[chr].xoff;
            }

            if (alignment == epoc::text_alignment::right) {
                cur_pos.x = text_box.size.x + text_box.top.x - static_cast<int>(size_length);
            } else {
                cur_pos.x += static_cast<int>((text_box.size.x - size_length) / 2);
            }
        }

        builder->set_blend_mode(true);
        builder->blend_formula(drivers::blend_equation::add, drivers::blend_equation::add,
            drivers::blend_factor::frag_out_alpha, drivers::blend_factor::one_minus_frag_out_alpha,
            drivers::blend_factor::zero, drivers::blend_factor::one);

        // Start to render these texts.
        for (auto &chr : text) {
            eka2l1::rect source_rect;
            adapter::character_info &info = characters_[chr];

            source_rect.top = { info.x0, info.y0 };
            source_rect.size = eka2l1::object_size(info.x1 - info.x0, info.y1 - info.y0);

            eka2l1::rect dest_rect;
            dest_rect.top.x = cur_pos.x + static_cast<int>(info.xoff);
            dest_rect.top.y = cur_pos.y + static_cast<int>(info.yoff);
            dest_rect.size.x = static_cast<int>(info.xoff2 - info.xoff);
            dest_rect.size.y = static_cast<int>(info.yoff2 - info.yoff);

            if ((dest_rect.size.x != 0) && (dest_rect.size.y != 0) && (source_rect.size.x != 0) && (source_rect.size.y != 0)) {
                builder->draw_bitmap(atlas_handle_, 0, dest_rect, source_rect, eka2l1::vec2(0, 0), 0.0f,
                    drivers::bitmap_draw_flag_use_brush);
            }

            // TODO: Newline
            cur_pos.x += static_cast<int>(std::round(info.xadv));
        }

        builder->set_blend_mode(false);

        return true;
    }
}