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
    
    GLint to_filter_option(const filter_option op) {
        switch (op) {
        case filter_option::linear:
            return GL_LINEAR;

        default:
            break;
        }

        return 0;
    }
}