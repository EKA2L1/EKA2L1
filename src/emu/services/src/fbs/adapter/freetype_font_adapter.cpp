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

#include <common/cvt.h>
#include <common/log.h>

#include <services/fbs/adapter/freetype_font_adapter.h>

#include <memory>

namespace eka2l1::epoc::adapter {
    struct freetype_lib_raii {
        FT_Library lib_{};

        explicit freetype_lib_raii() {
            auto err = FT_Init_FreeType(&lib_);
            if (err) {
                LOG_ERROR(SERVICE_FBS, "Failed to initialize FreeType library, error: {}", FT_Error_String(err));
            }
        }

        ~freetype_lib_raii() {
            FT_Done_FreeType(lib_);
        }
    };

    std::unique_ptr<freetype_lib_raii> ft_lib_raii_;

    FT_Library get_ft_lib() {
        return ft_lib_raii_->lib_;
    }

    freetype_font_adapter::freetype_font_adapter(std::vector<std::uint8_t> &data_)
        : is_valid_(false) {
        if (!ft_lib_raii_) {
            ft_lib_raii_ = std::make_unique<freetype_lib_raii>();
        }

        FT_Face query_face;
        auto err = FT_New_Memory_Face(get_ft_lib(), data_.data(), static_cast<FT_Long>(data_.size()), -1, &query_face);

        if (err) {
            LOG_ERROR(SERVICE_FBS, "FreeType failed to query face count from memory, error: {}", FT_Error_String(err));
            return;
        }

        for (auto i = 0; i < query_face->num_faces; i++) {
            FT_Face face;
            err = FT_New_Memory_Face(get_ft_lib(), data_.data(), static_cast<FT_Long>(data_.size()), i, &face);
            if (err) {
                LOG_ERROR(SERVICE_FBS, "FreeType failed to read face from memory, error: {}", FT_Error_String(err));
                continue;
            }
            faces_.push_back(face);
        }

        if (!faces_.empty()) {
            is_valid_ = true;
        }
    }

    std::uint32_t freetype_font_adapter::line_gap(const std::size_t idx) {
        if (idx >= faces_.size()) {
            return 0;
        }

        return faces_[idx]->size->metrics.height - faces_[idx]->size->metrics.ascender + faces_[idx]->size->metrics.descender;
    }

    bool freetype_font_adapter::get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) {
        return false;
    }

    bool freetype_font_adapter::get_glyph_metric(const std::size_t idx, std::uint32_t code, open_font_character_metric &character_metric, const std::int32_t baseline_horz_off, const std::uint32_t metric_identifier) {
        return false;
    }

    std::uint8_t *freetype_font_adapter::get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier, int *rasterized_width, int *rasterized_height, uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type) {
        return nullptr;
    }

    void freetype_font_adapter::free_glyph_bitmap(std::uint8_t *data) {
    }

    std::int32_t freetype_font_adapter::begin_get_atlas(std::uint8_t *atlas_ptr, const eka2l1::vec2 atlas_size) {
        return 0;
    }

    bool freetype_font_adapter::get_glyph_atlas(const std::int32_t handle, const std::size_t idx, const char16_t start_code, int *unicode_point, const char16_t num_code, const std::uint32_t metric_identifier, character_info *info) {
        return false;
    }

    void freetype_font_adapter::end_get_atlas(const std::int32_t handle) {
    }

    bool freetype_font_adapter::does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) {
        if (idx >= faces_.size()) {
            return false;
        }

        auto face = faces_[idx];

        if (code & 0x80000000) {
            return ((code & ~0x80000000) < face->num_glyphs);
        }

        return FT_Get_Char_Index(face, code) != 0;
    }

    std::size_t freetype_font_adapter::count() {
        return faces_.size();
    }

    bool freetype_font_adapter::has_character(const std::size_t face_index, const std::int32_t codepoint, const std::uint32_t metric_identifier) {
        if (face_index >= faces_.size()) {
            return false;
        }

        auto face = faces_[face_index];
        auto glyph_index = FT_Get_Char_Index(face, codepoint);

        return glyph_index != 0;
    }

    bool freetype_font_adapter::get_table_content(const std::size_t face_index, const std::uint32_t tag4, std::uint8_t *dest, uint32_t &dest_size) {
        return font_file_adapter_base::get_table_content(face_index, tag4, dest, dest_size);
    }

    std::optional<open_font_metrics> freetype_font_adapter::get_nearest_supported_metric(const std::size_t face_index, const std::uint16_t targeted_font_size, std::uint32_t *metric_identifier) {
        return std::optional<open_font_metrics>();
    }
}
