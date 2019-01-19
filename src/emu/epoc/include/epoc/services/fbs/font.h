/*
 * Copyright (c) 2019 EKA2L1 Team
 * 
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 * 
 * Initial contributor: pent0
 * Contributors:
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

#include <epoc/ptr.h>
#include <epoc/utils/des.h>

namespace eka2l1::epoc {
    struct typeface_info {
        epoc::buf_static<char16_t, 0x18> name;
        std::uint32_t flags;

        enum {
            tf_propotional = 1,
            tf_serif = 2,
            tf_symbol = 4
        };
    };

    struct font_style {
        enum {
            italic = 0x1,
            bold = 0x2,
            super = 0x4,
            sub = 0x8
        };

        // 16 bit (high) = bitmap type, font effects = (12 bit) middule, 4 bits low for style
        std::uint32_t flags;
        eka2l1::ptr<void> reserved1;
        eka2l1::ptr<void> reserved2;
    };

    struct font_spec {
        typeface_info tf;
        std::int32_t height;
        font_style style;
    };
    
    enum {
        DEAD_VTABLE = 0xDEAD1AB
    };

    struct alg_style {
        int baseline_offsets_in_pixel;

        enum {
            bold = 0x1,
            italic = 0x2,
            mono = 0x4
        };

        std::uint8_t flags;
        std::uint8_t width_factor;
        std::uint8_t height_factor;
    };

    struct bitmapfont {
        eka2l1::ptr<void> vtable;

        font_spec spec_in_twips;
        alg_style algorithic_style;

        eka2l1::ptr<void> allocator;
        int fontbitmap_offset;

        // This was not used by Symbian's by default i think
        // Qt generally access this itself by hardcoding offset of this
        eka2l1::ptr<void> openfont;

        std::uint32_t reserved;
        std::uint32_t font_uid;
    };
}