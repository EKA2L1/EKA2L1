#pragma once

namespace eka2l1::dispatch {
    enum {
        // Most GLES1 Symbian phone seems to only supported up to 2 slots (even iPhone 3G is so).
        // But be three for safety. Always feel 2 is too limited.
        GLES1_EMU_MAX_TEXTURE_SIZE = 4096,
        GLES1_EMU_MAX_TEXTURE_MIP_LEVEL = 10,
        GLES1_EMU_MAX_TEXTURE_COUNT = 3,
        GLES1_EMU_MAX_LIGHT = 8,
        GLES1_EMU_MAX_CLIP_PLANE = 6
    };

    enum gles1_static_string_key {
        GLES1_STATIC_STRING_KEY_VENDOR = 0x1F00,
        GLES1_STATIC_STRING_KEY_RENDERER,
        GLES1_STATIC_STRING_KEY_VERSION,
        GLES1_STATIC_STRING_KEY_EXTENSIONS
    };

    static constexpr const char *GLES1_STATIC_STRING_VENDOR = "EKA2L1";
    static constexpr const char *GLES1_STATIC_STRING_RENDERER = "POWERVR";
    static constexpr const char *GLES1_STATIC_STRING_EXTENSIONS = "";
}