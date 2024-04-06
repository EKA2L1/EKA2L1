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

#include <services/fbs/adapter/font_adapter.h>
#include <stb_truetype.h>

namespace eka2l1::epoc::adapter {
    class stb_font_file_adapter : public font_file_adapter_base {
    private:
        std::vector<std::uint8_t> data_;
        std::map<int, stbtt_fontinfo> cache_info;

        stbtt_fontinfo info_;
        common::identity_container<std::unique_ptr<stbtt_pack_context>> contexts_;

        std::size_t count_;
        std::uint8_t flags_;

        enum {
            FLAGS_CONTEXT_INITED = 1 << 0,
            FLAG_SUPPORT_STRUCT_INITED = 1 << 1
        };

    protected:
        std::uint32_t get_glyph_advance(const std::size_t face_index, const std::uint32_t codepoint, const std::uint32_t metric_identifier, const bool vertical = false) override;

    public:
        stbtt_fontinfo *get_or_create_info(const int idx, int *off);

        explicit stb_font_file_adapter(std::vector<std::uint8_t> &data_);
        bool is_valid() override {
            return flags_ & FLAGS_CONTEXT_INITED;
        }

        bool vectorizable() const override {
            return true;
        }

        std::uint32_t line_gap(const std::size_t idx, const std::uint32_t metric_identifier) override;

        bool get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) override;

        std::uint8_t *get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier,
            int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type,
            open_font_character_metric &character_metric) override;

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
            std::uint32_t *metric_identifier = nullptr, bool is_design_font_size = true) override;
    };
}
