#pragma once

namespace eka2l1::dispatch {
    enum {
        // Most GLES1 Symbian phone seems to only supported up to 2 slots (even iPhone 3G is so).
        // But be three for safety. Always feel 2 is too limited.
        GLES1_EMU_MAX_TEXTURE_SIZE = 4096,
        GLES1_EMU_MAX_TEXTURE_MIP_LEVEL = 10,
        GLES1_EMU_MAX_TEXTURE_COUNT = 3,
        GLES1_EMU_MAX_LIGHT = 8,
        GLES1_EMU_MAX_CLIP_PLANE = 6,
        GLES1_EMU_MAX_WEIGHTS_PER_VERTEX = 4,
        GLES1_EMU_MAX_PALETTE_MATRICES = 16
    };

    enum gles1_static_string_key {
        GLES1_STATIC_STRING_KEY_VENDOR = 0x1F00,
        GLES1_STATIC_STRING_KEY_RENDERER,
        GLES1_STATIC_STRING_KEY_VERSION,
        GLES1_STATIC_STRING_KEY_EXTENSIONS
    };

    static constexpr const char *GLES1_STATIC_STRING_VENDOR = "IMAGINATION TECHNOLOGIES EKA2L1";
    static constexpr const char *GLES1_STATIC_STRING_RENDERER = "POWERVR EKA2L1";
    static constexpr const char *GLES1_STATIC_STRING_VERSION = "1.1.0 POWERVR (EKA2L1 IMPLEMENTATION)";
    static constexpr const char *GLES1_STATIC_STRING_EXTENSIONS = "GL_IMG_texture_compression_pvrtc "
        "GL_OES_compressed_ETC1_RGB8_texture "
        "GL_OES_compressed_paletted_texture "
        "GL_ARB_texture_env_combine "
        "GL_OES_texture_env_crossbar "
        "GL_OES_texture_mirrored_repeat "
        "GL_EXT_texture_format_BGRA8888 "
        "GL_OES_vertex_buffer_object "
        "GL_EXT_texture_lod_bias "
        "GL_OES_matrix_palette ";
        /**
         * "GL_OES_framebuffer_object "
         * "GL_EXT_texture_compression_s3tc "
         */
}