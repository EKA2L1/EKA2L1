/*
 * Copyright (c) 2018 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
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

#include <cstdint>
#include <ptr.h>

namespace eka2l1::epoc::fbs {
    /* These flags can be directly transferred to FreeType's flags */
    enum {
        italic = 0x1,
        bold = 0x2,
        super = 0x4,
        sub = 0x8
    };

    struct font_style {
        uint32_t flags;
        eka2l1::ptr<void> reserved1;
        eka2l1::ptr<void> reserved2;
    };

    enum {
        typeface_proportional = 1,
        typeface_serif = 2,
        typeface_symbol = 4,
    };

    /*! \brief Represents a typeface.
     */
    struct typeface {
        uint32_t name_length;
        short name[0x18];

        uint32_t flags;
    };

    /*! \brief The space of the font. 
     *
     * Height must be in twips.
     */
    struct font_spec {
        typeface tf;
        int height;
        font_style style;
    };

    /*! \brief The font size info. 
     * 
     * In order for the server to find the right font, client must provide the max height the font can be,
     * and also the device size in twips.
     */
    struct font_size_info {
        uint32_t max_height;
        uint32_t device_width;
        uint32_t device_height;
    };

    /*! \brief The info of font in FBS server. 
     *
     * When an user requested to find a font, when the font is found, it is created and loaded to FBS server 
     * (as bitmap, i havent confirmed this yet). The font info is then wrote to the 1th argument of IPC calls.
     * The type face store manager looks up cache by comparing the address of the font bitmap (?). 
     */
    struct font_info {
        int font_handle;
        int font_offset;
        int server_handle;
    };

    struct alg_style {
        int baseline_off_pixels;
        uint8_t flags;
        uint8_t width_factor;
        uint8_t height_factor;
    };

    struct bitmap_font {
        font_spec spec_twips;
        alg_style style;
        eka2l1::ptr<void> heap;
        int font_bitmap_off;
        eka2l1::ptr<void> open_font;

        uint32_t reserved;
        uint32_t id;
    };

    struct open_font_metrics {
        uint16_t design_height;
        uint16_t ascent;
        uint16_t descent;
        uint16_t max_height;
        uint16_t max_depth;
        uint16_t max_width;
        uint16_t baseline_correction;
        uint16_t reserved;
    };

    struct open_font {
        eka2l1::ptr<void> heap;
    };
}