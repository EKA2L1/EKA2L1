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
#include <services/fbs/adapter/stb_font_adapter.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace eka2l1::epoc::adapter {
    static void free_stb_pack_context(std::unique_ptr<stbtt_pack_context> &ctx) {
        stbtt_PackEnd(ctx.get());
        ctx.reset();
    }

    static bool is_stb_pack_context_free(std::unique_ptr<stbtt_pack_context> &ctx) {
        return (ctx == nullptr);
    }

    stb_font_file_adapter::stb_font_file_adapter(std::vector<std::uint8_t> &data_)
        : data_(data_)
        , flags_(0)
        , contexts_(is_stb_pack_context_free, free_stb_pack_context) {
        count_ = stbtt_GetNumberOfFonts(&data_[0]);

        if (count_ > 0) {
            flags_ |= FLAGS_CONTEXT_INITED;
        }
    }

    std::size_t stb_font_file_adapter::count() {
        return count_;
    }

    stbtt_fontinfo *stb_font_file_adapter::get_or_create_info(const int idx, int *off) {
        if (idx >= count_) {
            return nullptr;
        }

        *off = stbtt_GetFontOffsetForIndex(&data_[0], static_cast<int>(idx));
        auto result = cache_info.find(*off);

        if (result != cache_info.end()) {
            return &result->second;
        }

        stbtt_fontinfo info;
        stbtt_InitFont(&info, &data_[0], *off);

        cache_info.emplace(*off, std::move(info));
        return &cache_info[*off];
    }

    bool stb_font_file_adapter::get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) {
        int off = 0;
        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(idx), &off);

        if (!info) {
            return false;
        }

        auto get_name = [&](const int id) -> std::u16string {
            int name_len = 0;

            const char *fname = stbtt_GetFontNameString(info, &name_len, STBTT_PLATFORM_ID_MICROSOFT, STBTT_MS_EID_UNICODE_BMP,
                STBTT_MS_LANG_ENGLISH, id);

            if (name_len && fname[0] == '\0') {
                fname += 1;
            }

            return std::u16string(reinterpret_cast<const char16_t *>(fname), name_len / 2);
        };

        face_attrib.name = get_name(4);
        face_attrib.fam_name = get_name(1);
        face_attrib.local_full_name = face_attrib.name;
        face_attrib.local_full_fam_name = face_attrib.fam_name;
        face_attrib.style = 0;

        // Get style
        const auto head_offset = stbtt__find_table(&data_[0], off, "head");
        const std::uint16_t opentype_style = *reinterpret_cast<const std::uint16_t *>(&data_[0] + off + head_offset + 44);

        if (opentype_style & 0x1) {
            face_attrib.style |= open_font_face_attrib::bold;
        }

        if (opentype_style & 0x2) {
            face_attrib.style |= open_font_face_attrib::italic;
        }

        // TODO: Serif flags
        int y0, y1 = 0;
        int x0_M, x1_M, x0_i, x1_i = 0;

        int res1 = stbtt_GetCodepointBox(info, 'M', &x0_M, &y0, &x1_M, &y1);
        int res2 = stbtt_GetCodepointBox(info, 'i', &x0_i, &y0, &x1_i, &y1);

        if (!res1 || !res2) {
            face_attrib.style |= open_font_face_attrib::symbol;
        } else {
            if (abs(x1_i - x0_i) == abs(x1_M - x0_M)) {
                face_attrib.style |= open_font_face_attrib::mono_width;
            }
        }

        const auto os2_off = stbtt__find_table(&data_[0], off, "OS/2");

        // This maybe optional, so let's check
        if (os2_off != 0) {
            // Copy unicode coverage in
            std::copy(reinterpret_cast<std::uint32_t *>(&data_[0] + off + os2_off + 42),
                reinterpret_cast<std::uint32_t *>(&data_[0] + off + os2_off + 42) + 4, face_attrib.coverage);

            // OS/2 field which indicates lowest size of the font. Not really sure, since those fields
            // were added since 2013..
            // https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6OS2.html
            // Offset 84 of the table. May need to reconfirm (i calculate offset in my head)
            if (off + os2_off + 84 + 2 < data_.size()) {
                face_attrib.min_size_in_pixels = *reinterpret_cast<const std::uint16_t *>(&data_[0] + off + os2_off + 84);
            }
        }

        if (face_attrib.min_size_in_pixels <= 0) {
            face_attrib.min_size_in_pixels = 1;
        }

        return true;
    }

    std::optional<open_font_metrics> stb_font_file_adapter::get_nearest_supported_metric(const std::size_t idx, const std::uint16_t target_font_size, std::uint32_t *metric_identifier) {
        int off = 0;
        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(idx), &off);

        if (!info) {
            return std::nullopt;
        }

        int gaps = 0;
        int x0, y0, x1, y1;

        stbtt_GetFontBoundingBox(info, &x0, &y0, &x1, &y1);

        const float scale_factor = stbtt_ScaleForPixelHeight(info, target_font_size);

        open_font_metrics metrics;

        // TODO: Compensate for aspect ratio. We currently don't have screen ratio, since
        //  no physical screen size is provided
        // By the way. Descent is negative (because it follows coordinate)
        metrics.ascent = static_cast<std::int16_t>(y1 * scale_factor);
        metrics.descent = static_cast<std::int16_t>(-y0 * scale_factor);
        metrics.max_height = static_cast<std::int16_t>((y1 - y0) * scale_factor);
        metrics.design_height = static_cast<std::int16_t>((y1 - y0) * scale_factor);
        metrics.max_width = static_cast<std::int16_t>((x1 - x0) * scale_factor);
        metrics.max_depth = static_cast<std::int16_t>(-y0 * scale_factor);
        metrics.baseline_correction = 0;

        if (metric_identifier != nullptr) {
            *metric_identifier = target_font_size;
        }

        return metrics;
    }

    bool stb_font_file_adapter::does_glyph_exist(const size_t idx, const uint32_t code, const std::uint32_t metric_identifier) {
        int off = 0;
        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(idx), &off);
        if (!info) {
            return false;
        }
        if (code & 0x80000000) {
            return (idx < info->numGlyphs);
        }
        return stbtt_FindGlyphIndex(info, code) != 0;
    }

    std::uint8_t *stb_font_file_adapter::get_glyph_bitmap(const std::size_t idx, std::uint32_t code,
        const std::uint32_t metric_identifier, int *rasterized_width, int *rasterized_height,
        std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type) {
        bool get_codepoint = true;
        const std::uint32_t font_size = metric_identifier;

        if (code & 0x80000000) {
            // It's truly the glyph index.
            code &= ~0x80000000;
            get_codepoint = false;
        }

        if (code == 0) {
            // Fallback character.
            code = '?';
        }

        int off = 0;
        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(idx), &off);

        if (!info) {
            return nullptr;
        }

        std::uint8_t *result;
        int x0, x1, y0, y1 = 0;

        stbtt_GetFontBoundingBox(info, &x0, &y0, &x1, &y1);
        const float scale_factor = stbtt_ScaleForPixelHeight(info, static_cast<float>(font_size));

        if (get_codepoint) {
            result = stbtt_GetCodepointBitmap(info, scale_factor, scale_factor, static_cast<int>(code), rasterized_width,
                rasterized_height, nullptr, nullptr);
        } else {
            result = stbtt_GetGlyphBitmap(info, scale_factor, scale_factor, static_cast<int>(code), rasterized_width,
                rasterized_height, nullptr, nullptr);
        }

        if (bmp_type) {
            *bmp_type = epoc::glyph_bitmap_type::antialised_glyph_bitmap;
        }

        if (result) {
            total_size = *rasterized_width * *rasterized_height;
        }

        return result;
    }

    bool stb_font_file_adapter::get_glyph_metric(const std::size_t idx, std::uint32_t code, open_font_character_metric &character_metric, const std::int32_t baseline_horz_off,
        const std::uint32_t metric_identifier) {
        const std::uint32_t font_size = metric_identifier;
        bool get_codepoint = true;

        if (code & 0x80000000) {
            // It's truly the glyph index.
            code &= ~0x80000000;
            get_codepoint = false;
        }

        if (code == 0) {
            // Fallback character.
            code = '?';
        }

        int off = 0;
        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(idx), &off);

        if (!info) {
            return false;
        }

        int adv_width = 0;
        int left_side_bearing = 0;
        int x0, x1, y0, y1 = 0;

        if (get_codepoint) {
            stbtt_GetCodepointHMetrics(info, static_cast<int>(code), &adv_width, &left_side_bearing);
            stbtt_GetCodepointBox(info, static_cast<int>(code), &x0, &y0, &x1, &y1);
        } else {
            stbtt_GetGlyphHMetrics(info, static_cast<int>(code), &adv_width, &left_side_bearing);
            stbtt_GetGlyphBox(info, static_cast<int>(code), &x0, &y0, &x1, &y1);
        }

        const float scale_factor = stbtt_ScaleForPixelHeight(info, static_cast<float>(font_size));

        float scaled_x0 = std::floor(static_cast<float>(x0) * scale_factor);
        float scaled_y0 = std::floor(static_cast<float>(-y1) * scale_factor);
        float scaled_x1 = std::ceil(static_cast<float>(x1) * scale_factor);
        float scaled_y1 = std::ceil(static_cast<float>(-y0) * scale_factor);

        character_metric.width = static_cast<std::int16_t>(scaled_x1 - scaled_x0);
        character_metric.height = static_cast<std::int16_t>(scaled_y1 - scaled_y0);
        character_metric.horizontal_advance = static_cast<std::int16_t>(std::roundf(adv_width * scale_factor));
        character_metric.horizontal_bearing_x = static_cast<std::int16_t>(left_side_bearing * scale_factor);

        // Let's calculate vertical advance. Every character of the font should have same vertical size.
        // So use getFontVMetrics
        int ascent = 0;
        int descent = 0;
        int linegap = 0;

        stbtt_GetFontVMetrics(info, &ascent, &descent, &linegap);

        // Calculate vertical advance by char_ascent - char_descent + linegap
        character_metric.vertical_advance = static_cast<std::int16_t>(scaled_y1 - scaled_y0 + linegap * scale_factor);
        character_metric.horizontal_bearing_y = static_cast<std::int16_t>(scaled_y1);
        character_metric.horizontal_bearing_y = static_cast<std::int16_t>(scaled_y1);

        // Not caring about vertical placement right now (text placement)
        character_metric.vertical_bearing_y = 0;
        character_metric.vertical_bearing_x = 0;

        return true;
    }

    std::uint32_t stb_font_file_adapter::line_gap(const std::size_t idx, const std::uint32_t metric_identifier) {
        int ascent, descent, linegap = 0;
        int off = 0;

        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(idx), &off);
        stbtt_GetFontVMetrics(info, &ascent, &descent, &linegap);

        const float scale_factor = stbtt_ScaleForPixelHeight(info, static_cast<float>(metric_identifier));
        return static_cast<std::uint32_t>(linegap * scale_factor);
    }

    void stb_font_file_adapter::free_glyph_bitmap(std::uint8_t *data) {
        stbtt_FreeBitmap(data, nullptr);
    }

    std::int32_t stb_font_file_adapter::begin_get_atlas(std::uint8_t *atlas_ptr, const eka2l1::vec2 atlas_size) {
        std::unique_ptr<stbtt_pack_context> context = std::make_unique<stbtt_pack_context>();
        if (stbtt_PackBegin(context.get(), atlas_ptr, atlas_size.x, atlas_size.y, 0, 1, nullptr) == 0) {
            return -1;
        }

        return static_cast<std::int32_t>(contexts_.add(context));
    }

    void stb_font_file_adapter::end_get_atlas(const std::int32_t handle) {
        contexts_.remove(static_cast<std::size_t>(handle));
    }

    bool stb_font_file_adapter::get_glyph_atlas(const std::int32_t handle, const std::size_t idx, const char16_t start_code, int *unicode_point,
        const char16_t num_code, const std::uint32_t font_size, character_info *info) {
        auto character_infos = std::make_unique<stbtt_packedchar[]>(num_code);
        std::unique_ptr<stbtt_pack_context> *context_ptr = contexts_.get(handle);

        if (!context_ptr) {
            return false;
        }

        stbtt_pack_context *context = context_ptr->get();
        stbtt_PackSetOversampling(context, 2, 2);

        stbtt_pack_range range;
        range.array_of_unicode_codepoints = unicode_point;
        range.chardata_for_range = character_infos.get();
        range.font_size = static_cast<float>(font_size);
        range.num_chars = num_code;
        range.first_unicode_codepoint_in_range = start_code;

        if (!stbtt_PackFontRanges(context, data_.data(), static_cast<int>(idx), &range, 1)) {
            return false;
        }

        if (info) {
            std::memcpy(info, character_infos.get(), num_code * sizeof(character_info));
        }

        return true;
    }

    bool stb_font_file_adapter::has_character(const std::size_t face_index, const std::int32_t codepoint, const std::uint32_t font_size) {
        int off = 0;
        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(face_index), &off);

        if (!info) {
            return false;
        }

        return (stbtt_FindGlyphIndex(info, codepoint) != 0);
    }
    
    std::uint32_t stb_font_file_adapter::get_glyph_advance(const std::size_t face_index, const std::uint32_t codepoint, const std::uint32_t font_size, const bool vertical) {
        int off = 0;
        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(face_index), &off);

        if (!info) {
            return 0xFFFFFFFF;
        }

        int adv_width, left_side_bearing = 0;
        stbtt_GetCodepointHMetrics(info, static_cast<int>(codepoint), &adv_width, &left_side_bearing);

        int wx0, wx1, wy0, wy1 = 0;
        stbtt_GetFontBoundingBox(info, &wx0, &wy0, &wx1, &wy1);

        const float scale_factor = stbtt_ScaleForPixelHeight(info, static_cast<float>(font_size));
        return static_cast<std::uint32_t>(adv_width * scale_factor);
    }

    // Forked from original stbtt
    static std::pair<stbtt_uint32, stbtt_uint32> stbtt__find_table_with_len(stbtt_uint8 *data, stbtt_uint32 fontstart, stbtt_uint32 tag) {
        stbtt_int32 num_tables = ttUSHORT(data+fontstart+4);
        stbtt_uint32 tabledir = fontstart + 12;
        stbtt_int32 i;
        for (i=0; i < num_tables; ++i) {
            stbtt_uint32 loc = tabledir + 16*i;
            if (ttULONG(data+loc+0) == tag)
                return std::make_pair(ttULONG(data+loc+8), ttULONG(data+loc+12));
        }
        return std::make_pair(0, 0);
    }

    bool stb_font_file_adapter::get_table_content(const std::size_t face_index, const std::uint32_t tag4, std::uint8_t *dest,
        std::uint32_t &dest_size) {
        int off = 0;
        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(face_index), &off);

        if (!info) {
            return false;
        }

        std::pair<stbtt_uint32, stbtt_uint32> res = stbtt__find_table_with_len(&data_[0], off, tag4);
        if (res.first == 0) {
            return false;
        }

        if (!dest) {
            dest_size = res.second;
            return true;
        }

        dest_size = common::min<std::uint32_t>(res.second, dest_size);
        std::memcpy(dest, &data_[0] + res.first + off, dest_size);

        return true;
    }
}
