#include <drivers/graphics/backend/ogl/common_ogl.h>

namespace eka2l1::drivers {
    GLenum data_format_to_gl_enum(const data_format format) {
        switch (format) {
        case data_format::byte:
            return GL_UNSIGNED_BYTE;
            break;

        case data_format::sbyte:
            return GL_BYTE;
            break;

        case data_format::sword:
            return GL_SHORT;
            break;

        case data_format::word:
            return GL_UNSIGNED_SHORT;
            break;

        case data_format::uint:
            return GL_UNSIGNED_INT;
            break;

        case data_format::sint:
            return GL_INT;
            break;

        case data_format::sfloat:
            return GL_FLOAT;
            break;

        default:
            break;
        }

        return GL_INVALID_ENUM;
    }
    
    GLint texture_format_to_gl_enum(const texture_format format) {
        switch (format) {
        case texture_format::r:
            return GL_RED;

        case texture_format::r8:
            return GL_R8;

        case texture_format::rg:
            return GL_RG;

        case texture_format::rg8:
            return GL_RG8;

        case texture_format::rgb:
            return GL_RGB;

        case texture_format::bgr:
            return GL_BGR;

        case texture_format::bgra:
            return GL_BGRA;

        case texture_format::rgba:
            return GL_RGBA;

        case texture_format::rgba4:
            return GL_RGBA4;

        case texture_format::depth24_stencil8:
            return GL_DEPTH24_STENCIL8;

        case texture_format::depth_stencil:
            return GL_DEPTH_STENCIL;

        default:
            break;
        }

        return 0;
    }

    GLint texture_data_type_to_gl_enum(const texture_data_type data_type) {
        switch (data_type) {
        case texture_data_type::ubyte:
            return GL_UNSIGNED_BYTE;

        case texture_data_type::ushort:
            return GL_UNSIGNED_SHORT;

        case texture_data_type::uint_24_8:
            return GL_UNSIGNED_INT_24_8;

        case texture_data_type::ushort_4_4_4_4:
            return GL_UNSIGNED_SHORT_4_4_4_4;

        case texture_data_type::ushort_5_6_5:
            return GL_UNSIGNED_SHORT_5_6_5;

        case texture_data_type::ushort_5_5_5_1:
            return GL_UNSIGNED_SHORT_5_5_5_1;

        default:
            break;
        }

        return 0;
    }

    GLint to_filter_option(const filter_option op) {
        switch (op) {
        case filter_option::linear:
            return GL_LINEAR;

        case filter_option::nearest:
            return GL_NEAREST;

        default:
            break;
        }

        return 0;
    }
}