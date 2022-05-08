#pragma once

#include <dispatch/libraries/gles_shared/consts.h>

namespace eka2l1::dispatch {
    enum {
        // Survey shows most provide 16. But use the minimum here, as the hardware probably can't provide
        // more at the time (PowerVR GPU) (for both these one wrote here)
        GLES2_EMU_MAX_VERTEX_ATTRIBS_COUNT = 8,
        GLES2_EMU_MAX_TEXTURE_UNIT_COUNT = 8
    };
    
    static constexpr const char *GLES2_STATIC_STRING_VENDOR = "IMAGINATION TECHNOLOGIES EKA2L1";
    static constexpr const char *GLES2_STATIC_STRING_RENDERER = "POWERVR EKA2L1";
    static constexpr const char *GLES2_STATIC_STRING_VERSION = "2.0.0 POWERVR (EKA2L1 IMPLEMENTATION)";
    static constexpr const char *GLES2_STATIC_STRING_EXTENSIONS = "GL_IMG_texture_compression_pvrtc "
        "GL_OES_compressed_ETC1_RGB8_texture "
        "GL_OES_compressed_paletted_texture "
        "GL_EXT_texture_format_BGRA8888 "
        "GL_OES_vertex_buffer_object "
        "GL_EXT_texture_lod_bias "
        "GL_OES_framebuffer_object "
        "GL_OES_packed_depth_stencil ";
        /*
         * "GL_EXT_texture_compression_s3tc "
         */
    static constexpr const char *GLES2_STATIC_STRING_SHADER_LANGUAGE_VERSION = "1.0 ES";
}