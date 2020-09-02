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

#pragma once

#include <services/fbs/adapter/font_adapter.h>
#include <stb_rect_pack.h>

#include <common/buffer.h>
#include <common/container.h>
#include <loader/gdr.h>

#include <vector>

namespace eka2l1::epoc::adapter {
    struct gdr_font_atlas_pack_context {
        std::vector<stbrp_node> pack_nodes_;
        std::unique_ptr<stbrp_context> pack_context_;
        std::uint8_t *pack_dest_;
        eka2l1::vec2 pack_size_;
    };

    class gdr_font_file_adapter : public font_file_adapter_base {
        loader::gdr::file_store store_;
        std::unique_ptr<common::ro_stream> buf_stream_;

        std::vector<std::uint32_t*> dynamic_alloc_list_;
        common::identity_container<gdr_font_atlas_pack_context> pack_contexts_;

    protected:
        loader::gdr::character *get_character(const std::size_t idx, std::uint32_t code);

    public:
        explicit gdr_font_file_adapter(std::vector<std::uint8_t> &data_);

        bool is_valid() override {
            // This is valid if the stream still exists
            return buf_stream_.get();
        }

        bool get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) override;
        bool get_metrics(const std::size_t idx, open_font_metrics &metrics) override;
        bool get_glyph_metric(const std::size_t idx, std::uint32_t code,
            open_font_character_metric &character_metric, const std::int32_t baseline_horz_off,
            const std::uint16_t font_size) override;

        std::uint8_t *get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint16_t font_size,
            int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type) override;

        void free_glyph_bitmap(std::uint8_t *data) override;

        std::int32_t begin_get_atlas(std::uint8_t *atlas_ptr, const eka2l1::vec2 atlas_size) override;

        bool get_glyph_atlas(const std::int32_t handle, const std::size_t idx, const char16_t start_code, int *unicode_point,
            const char16_t num_code, const int font_size, character_info *info) override;

        void end_get_atlas(const std::int32_t handle) override;

        glyph_bitmap_type get_output_bitmap_type() const override {
            return monochrome_glyph_bitmap;
        }

        bool does_glyph_exist(std::size_t idx, std::uint32_t code) override;

        std::size_t count() override;
        std::uint32_t unique_id(const std::size_t face_index) override;
    };
}
