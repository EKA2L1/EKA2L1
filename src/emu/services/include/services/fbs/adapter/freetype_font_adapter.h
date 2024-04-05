/*
* Copyright (c) 2019 EKA2L1 Team.
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

#pragma once

#include <map>
#include <memory>
#include <vector>

#include <common/container.h>

#include <freetype/freetype.h>
#include <services/fbs/adapter/font_adapter.h>
#include <stb_rect_pack.h>

namespace eka2l1::epoc::adapter {
   class freetype_font_adapter : public font_file_adapter_base {
   private:
       struct atlas_pack_state {
           std::uint8_t *atlas_base_;
           eka2l1::vec2 atlas_size_;
           std::vector<stbrp_node> atlas_node_;
           stbrp_context atlas_context_;
       };

       std::vector<std::uint8_t> data_;
       std::vector<FT_Face> faces_;
       bool is_valid_;

       common::identity_container<std::unique_ptr<atlas_pack_state>> pack_states_;

   protected:
       std::uint32_t get_glyph_advance(const std::size_t face_index, const std::uint32_t codepoint, const std::uint32_t metric_identifier, const bool vertical = false) override;

   public:
       explicit freetype_font_adapter(std::vector<std::uint8_t> &data_);
       ~freetype_font_adapter() override;

       bool is_valid() override {
           return is_valid_;
       }

       bool vectorizable() const override {
           return true;
       }

       std::uint32_t line_gap(const std::size_t idx, const std::uint32_t metric_identifier) override;

       bool get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) override;
       bool get_glyph_metric(const std::size_t idx, std::uint32_t code,
           open_font_character_metric &character_metric, const std::int32_t baseline_horz_off,
           const std::uint32_t metric_identifier) override;

       std::uint8_t *get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier,
           int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type) override;

       void free_glyph_bitmap(std::uint8_t *data) override;

       std::int32_t begin_get_atlas(std::uint8_t *atlas_ptr, const eka2l1::vec2 atlas_size) override;

       bool get_glyph_atlas(const std::int32_t handle, const std::size_t idx, const char16_t start_code, int *unicode_point, const char16_t num_code,
           const std::uint32_t metric_identifier, character_info *info) override;

       void end_get_atlas(const std::int32_t handle) override;

       glyph_bitmap_type get_output_bitmap_type() const override {
           return antialised_glyph_bitmap;
       }

       bool does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) override;

       std::size_t count() override;

       std::optional<open_font_metrics> get_metric_with_uid(const std::size_t face_index, const std::uint32_t uid,
           std::uint32_t *metric_identifier) override {
           return std::nullopt;
       }

       bool has_character(const std::size_t face_index, const std::int32_t codepoint, const std::uint32_t metric_identifier) override;
       bool get_table_content(const std::size_t face_index, const std::uint32_t tag4, std::uint8_t *dest,
           std::uint32_t &dest_size) override;

       std::optional<open_font_metrics> get_nearest_supported_metric(const std::size_t face_index, const std::uint16_t targeted_font_size,
           std::uint32_t *metric_identifier = nullptr) override;
   };
}
