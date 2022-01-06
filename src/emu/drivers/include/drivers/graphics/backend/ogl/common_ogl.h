#pragma once

#include <drivers/graphics/common.h>
#include <glad/glad.h>

namespace eka2l1::drivers {
    GLenum data_format_to_gl_enum(const data_format format);
    GLint texture_format_to_gl_enum(const texture_format format);
    GLint texture_data_type_to_gl_enum(const texture_data_type data_type);
    GLint to_filter_option(const filter_option op);
    GLenum to_tex_parameter_enum(const addressing_direction dir);
    GLint to_tex_wrapping_enum(const addressing_option opt);
}