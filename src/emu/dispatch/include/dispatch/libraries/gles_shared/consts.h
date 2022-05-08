/*
 * Copyright (c) 2022 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
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

namespace eka2l1::dispatch {
    enum {
        GLES_EMU_MAX_TEXTURE_SIZE = 4096,
        GLES_EMU_MAX_TEXTURE_MIP_LEVEL = 10
    };

    enum gles_static_string_key {
        GLES_STATIC_STRING_KEY_VENDOR = 0x1F00,
        GLES_STATIC_STRING_KEY_RENDERER,
        GLES_STATIC_STRING_KEY_VERSION,
        GLES_STATIC_STRING_KEY_EXTENSIONS,
        GLES_STATIC_STRING_KEY_VENDOR_ES2,
        GLES_STATIC_STRING_KEY_RENDERER_ES2,
        GLES_STATIC_STRING_KEY_VERSION_ES2,
        GLES_STATIC_STRING_KEY_EXTENSIONS_ES2,
        GLES_STATIC_STRING_SHADER_LANGUAGE_VERSION_ES2 = 0x8B8C
    };

    using gl_fixed = std::int32_t;
}