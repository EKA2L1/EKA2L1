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
        : data_(data_) {
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
}
