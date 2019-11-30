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

#include <epoc/services/fbs/adapter/stb_font_adapter.h>
#include <common/cvt.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace eka2l1::epoc::adapter {
    stb_font_file_adapter::stb_font_file_adapter(std::vector<std::uint8_t> &data_)
        : data_(data_)
        , flags_(0) {
        count_ = stbtt_GetNumberOfFonts(&data_[0]);
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

            return std::u16string(reinterpret_cast<const char16_t*>(fname), name_len / 2);
        };

        face_attrib.name = get_name(4);
        face_attrib.fam_name = get_name(1);
        face_attrib.local_full_name = face_attrib.name;
        face_attrib.local_full_fam_name = face_attrib.fam_name;
        face_attrib.style = 0;

        // Get style
        const auto head_offset = stbtt__find_table(&data_[0], off, "head");
        const std::uint16_t opentype_style = *reinterpret_cast<const std::uint16_t*>(&data_[0] + off + head_offset + 44);
        
        if (opentype_style & 0x1) {
            face_attrib.style |= open_font_face_attrib::bold;
        }

        if (opentype_style & 0x2) {
            face_attrib.style |= open_font_face_attrib::italic;
        }

        // TODO: Serif and monotype flags
        
        const auto os2_off = stbtt__find_table(&data_[0], off, "OS/2");

        // This maybe optional, so let's check
        if (os2_off != 0) {
            // Copy unicode coverage in
            std::copy(reinterpret_cast<std::uint32_t*>(&data_[0] + off + os2_off + 33), 
                reinterpret_cast<std::uint32_t*>(&data_[0] + off + os2_off + 33) + 4, face_attrib.coverage);

            // OS/2 field which indicates lowest size of the font. Not really sure, since those fields
            // were added since 2013..
            // https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6OS2.html
            // Offset 84 of the table. May need to reconfirm (i calculate offset in my head)
            if (off + os2_off + 84 + 2 < data_.size()) {
                face_attrib.min_size_in_pixels = *reinterpret_cast<const std::uint16_t*>(&data_[0] + off + os2_off + 84);
            }
        }

        return true;
    }
    
    bool stb_font_file_adapter::get_metrics(const std::size_t idx, open_font_metrics &metrics) {
        int off = 0;
        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(idx), &off);

        if (!info) {
            return false;
        }

        int ascent = 0;
        int descent = 0;
        int gaps = 0;

        stbtt_GetFontVMetrics(info, &ascent, &descent, &gaps);

        // TODO: Compensate for aspect ratio. We currently don't have screen ratio, sincce
        // no physical screen size is provided
        metrics.ascent = static_cast<std::uint16_t>(ascent);
        metrics.descent = static_cast<std::uint16_t>(-descent);
        metrics.max_height = metrics.ascent + metrics.descent;
        int x0, y0, x1, y1;

        stbtt_GetFontBoundingBox(info, &x0, &y0, &x1, &y1);

        metrics.design_height = y1 - y0;
        metrics.max_width = x1 - x0;
        metrics.max_depth = metrics.descent;
        metrics.baseline_correction = 0;

        return true;
    }

    std::uint8_t *stb_font_file_adapter::get_glyph_bitmap(const std::size_t idx, std::uint32_t code,
        const float scale_x, const float scale_y, int *rasterized_width, int *rasterized_height,
        epoc::glyph_bitmap_type *bmp_type) {
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
            return nullptr;
        }

        std::uint8_t *result = nullptr;

        if (get_codepoint) {
            result = stbtt_GetCodepointBitmap(info, scale_x, scale_y, static_cast<int>(code), rasterized_width,
                rasterized_height, nullptr, nullptr);
        } else {
            result = stbtt_GetGlyphBitmap(info, scale_x, scale_y, static_cast<int>(code), rasterized_width,
                rasterized_height, nullptr, nullptr);
        }

        if (bmp_type) {
            *bmp_type = epoc::glyph_bitmap_type::antialised_glyph_bitmap;
        }

        return result;
    }

    bool stb_font_file_adapter::get_glyph_metric(const std::size_t idx, std::uint32_t code, 
        open_font_character_metric &character_metric, const std::int32_t baseline_horz_off, 
        const float scale_x, const float scale_y) {
        bool get_codepoint = true;

        if (code & 0x80000000) {
            // It's truly the glyph index.
            code &= ~0x80000000;
            get_codepoint = false;
        }

        if (code == 0) {
            // Fallback character.
            // I honestly love you! Let me tell me this to you...
            code = '?';
        }

        // And I'm sure you love me! It's going to get through
        int off = 0;
        stbtt_fontinfo *info = get_or_create_info(static_cast<int>(idx), &off);

        if (!info) {
            return false;
        }

        int adv_width = 0;
        int left_side_bearing = 0;
        int x0, x1, y0, y1 = 0;

        // Let's look for a glass slipper that fit you
        if (get_codepoint) {
            stbtt_GetCodepointHMetrics(info, static_cast<int>(code), &adv_width, &left_side_bearing);
            stbtt_GetCodepointBox(info, static_cast<int>(code), &x0, &y0, &x1, &y1);
        } else {
            stbtt_GetGlyphHMetrics(info, static_cast<int>(code), &adv_width, &left_side_bearing);
            stbtt_GetGlyphBox(info, static_cast<int>(code), &x0, &y0, &x1, &y1);
        }

        character_metric.width = static_cast<std::int16_t>(std::round((x1 - x0) * scale_x));
        character_metric.height = static_cast<std::int16_t>(std::round((y1 - y0) * scale_y));
        character_metric.horizontal_advance = static_cast<std::int16_t>(std::round(adv_width * scale_x));
        character_metric.horizontal_bearing_x = static_cast<std::int16_t>(std::round(left_side_bearing * scale_x));

        // As two! Step and go! ...
        // Let's calculate vertical advance. Every character of the font should have same vertical size.
        // So use getFontVMetrics
        int ascent = 0;
        int descent = 0;
        int linegap = 0;

        stbtt_GetFontVMetrics(info, &ascent, &descent, &linegap);

        // Calculate verical advance by ascent - descent + linegap
        character_metric.vertical_advance = static_cast<std::int16_t>(std::round(ascent - descent + linegap) * scale_y);
        
        // FreeType 2 rasterizer on WINS fill this as 0, so I guess this is ok?
        character_metric.vertical_bearing_y = 0;
        character_metric.vertical_bearing_x = 0;
        
        // I use image at here as reference
        // https://www.freetype.org/freetype2/docs/tutorial/step2.html
        character_metric.horizontal_bearing_y = static_cast<std::int16_t>(std::round(y1 *
            scale_y) - baseline_horz_off);

        return true;
    }

    void stb_font_file_adapter::free_glyph_bitmap(std::uint8_t *data) {
        stbtt_FreeBitmap(data, nullptr);
    }

    bool stb_font_file_adapter::begin_get_atlas(std::uint8_t *atlas_ptr, const eka2l1::vec2 atlas_size) {
        if (!stbtt_PackBegin(&context_, atlas_ptr, atlas_size.x, atlas_size.y, 0, 1, nullptr)) {
            return false;
        }

        return true;
    }

    void stb_font_file_adapter::end_get_atlas() {
        stbtt_PackEnd(&context_);
    }

    bool stb_font_file_adapter::get_glyph_atlas(const char16_t start_code, int *unicode_point,
        const char16_t num_code, const int font_size, character_info *info) {
        auto character_infos = std::make_unique<stbtt_packedchar[]>(num_code);

        stbtt_PackSetOversampling(&context_, 2, 2);

        stbtt_pack_range range;
        range.array_of_unicode_codepoints = unicode_point;
        range.chardata_for_range = character_infos.get();
        range.font_size = static_cast<float>(font_size);
        range.num_chars = num_code;
        range.first_unicode_codepoint_in_range = start_code;

        if (!stbtt_PackFontRanges(&context_, data_.data(), 0, &range, 1)) {
            return false;
        }

        if (info) {
            std::memcpy(info, character_infos.get(), num_code * sizeof(character_info));
        }

        return true;
    }
}
