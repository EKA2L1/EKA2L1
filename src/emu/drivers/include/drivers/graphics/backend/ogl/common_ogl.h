#pragma once

#include <drivers/graphics/common.h>
#include <glad/glad.h>

namespace eka2l1::drivers {
    GLenum data_format_to_gl_enum(const data_format format);
}